#include <Geode/modify/EditorUI.hpp>
#include <nodes.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <functional>
#include <string>

using namespace geode;
using namespace cocos2d;
using geode::cocos::CCArrayExt;

// ---------------------------------------------------------------------------
// Circle Tool
//
// Duplicates the current selection repeatedly around an arc, offering four
// distinct duplication "Modes" (Circle / Spiral / Wave / Random) plus a
// shared set of shaping parameters. The popup is organized into a mode
// sidebar and a two-tab content panel (Basic / Advanced) so the surface a
// user sees at any one time stays small and predictable, no matter how many
// parameters exist under the hood.
// ---------------------------------------------------------------------------

enum class ToolMode {
	Circle = 0,
	Spiral = 1,
	Wave = 2,
	Random = 3,
};

// Deterministic, allocation-free pseudo-random value in [-1, 1], based on a
// seed and an index. Using a hash instead of a stateful RNG means the result
// only depends on (seed, index), so re-running with the same seed always
// produces the same layout.
static float pseudo_random(int seed, int index) {
	uint32_t x = static_cast<uint32_t>(seed) * 747796405u + static_cast<uint32_t>(index) * 2891336453u + 1u;
	x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
	x = (x >> 22u) ^ x;
	return (static_cast<float>(x % 2001u) / 1000.f) - 1.f;
}

class CircleToolPopup : public geode::Popup {
public:
	// --- persisted parameters (kept across popup open/close) ---
	static ToolMode m_mode;
	static float m_angle;          // Arc, degrees
	static float m_step;           // Step, degrees
	static bool m_direction_cw;    // true = clockwise, false = counter-clockwise

	static bool m_split_squish;    // false = single "Squish" value, true = separate X/Y
	static float m_squish;         // used when m_split_squish == false
	static float m_squish_x;       // used when m_split_squish == true
	static float m_squish_y;

	static float m_center_x;       // pivot offset, relative to selection center
	static float m_center_y;

	static float m_spiral_turns;
	static float m_spiral_start_percent; // 0-100, starting radius as % of full squish

	static float m_wave_frequency;  // oscillations across the arc
	static float m_wave_amplitude;  // extra +/- radius fraction

	static float m_random_jitter;   // +/- radius fraction
	static int m_random_seed;

	static bool m_advanced_tab_active;

private:
	// --- layout / interactive nodes ---
	CCNode* m_sidebar = nullptr;
	CCNode* m_content = nullptr;
	CCLabelBMFont* m_mode_hint = nullptr;
	CCLabelBMFont* m_stats_label = nullptr;

	CCNode* m_basic_tab = nullptr;
	CCNode* m_advanced_tab = nullptr;

	CCNode* m_squish_uniform_node = nullptr;
	CCNode* m_squish_split_node = nullptr;

	CCNode* m_spiral_extra = nullptr;
	CCNode* m_wave_extra = nullptr;
	CCNode* m_random_extra = nullptr;
	CCNode* m_circle_extra = nullptr;

	std::vector<CCMenuItemSpriteExtra*> m_mode_buttons;
	std::vector<CCMenuItemSpriteExtra*> m_squish_buttons;
	std::vector<CCMenuItemSpriteExtra*> m_direction_buttons;
	std::vector<CCMenuItemSpriteExtra*> m_page_buttons;

public:
	static CircleToolPopup* create() {
		auto* node = new CircleToolPopup();
		if (node->init()) {
			node->autorelease();
			return node;
		} else {
			delete node;
			return nullptr;
		}
	}

	// ---------------------------------------------------------------------
	// small layout helpers
	// ---------------------------------------------------------------------

	static CCLabelBMFont* add_label(CCNode* parent, const char* text, float scale, CCPoint offset, ccColor3B color = {255, 255, 255}) {
		auto* label = NodeFactory<CCLabelBMFont>::start(text, "chatFont.fnt")
			.setScale(scale)
			.setColor(color)
			.end();
		parent->addChildAtPosition(label, Anchor::Center, offset);
		return label;
	}

	// Adds a "label above / input below" field and returns the TextInput.
	TextInput* add_field(CCNode* parent, const char* label_text, CCPoint pos, float width, std::string const& initial, std::function<void(std::string const&)> on_change) {
		add_label(parent, label_text, 0.4f, {pos.x, pos.y + 17.f}, {170, 210, 255});
		auto* input = geode::TextInput::create(width, "");
		input->setCommonFilter(CommonFilter::Float);
		input->setString(initial);
		input->setCallback(std::move(on_change));
		parent->addChildAtPosition(input, Anchor::Center, pos);
		return input;
	}

	CCMenuItemSpriteExtra* make_tab_button(const char* text, float width, SEL_MenuHandler sel) {
		return CCMenuItemSpriteExtra::create(
			ButtonSprite::create(text, width, true, "goldFont.fnt", "GJ_button_01.png", 0, 0.5f),
			this, sel
		);
	}

	static void set_active_state(CCMenuItemSpriteExtra* button, bool active) {
		button->setOpacity(active ? 255 : 130);
		button->setScale(active ? 1.05f : 0.95f);
	}

	// ---------------------------------------------------------------------
	// init
	// ---------------------------------------------------------------------

	bool init() override {
		if (!Popup::init(440.f, 360.f)) return false;

		this->setTitle("Circle Tool");

		auto* layer = m_mainLayer;
		auto* menu = m_buttonMenu;

		// ---- panel backgrounds -------------------------------------------------
		m_sidebar = CCNode::create();
		m_sidebar->setContentSize({112.f, 258.f});
		auto* sidebar_bg = CCLayerColor::create(ccc4(20, 20, 30, 170), 112.f, 258.f);
		sidebar_bg->setAnchorPoint({0.5f, 0.5f});
		m_sidebar->addChildAtPosition(sidebar_bg, Anchor::Center);
		layer->addChildAtPosition(m_sidebar, Anchor::Center, ccp(-150.f, -6.f));

		m_content = CCNode::create();
		m_content->setContentSize({282.f, 258.f});
		auto* content_bg = CCLayerColor::create(ccc4(14, 14, 22, 130), 282.f, 258.f);
		content_bg->setAnchorPoint({0.5f, 0.5f});
		m_content->addChildAtPosition(content_bg, Anchor::Center);
		layer->addChildAtPosition(m_content, Anchor::Center, ccp(68.f, -6.f));

		// ---- sidebar: mode selection -------------------------------------------
		add_label(m_sidebar, "MODE", 0.42f, ccp(0.f, 112.f), {255, 220, 120});

		const char* mode_names[4] = {"Circle", "Spiral", "Wave", "Random"};
		const float mode_ys[4] = {78.f, 40.f, 2.f, -36.f};
		SEL_MenuHandler mode_sels[4] = {
			menu_selector(CircleToolPopup::on_mode_circle),
			menu_selector(CircleToolPopup::on_mode_spiral),
			menu_selector(CircleToolPopup::on_mode_wave),
			menu_selector(CircleToolPopup::on_mode_random),
		};
		for (int i = 0; i < 4; ++i) {
			auto* btn = make_tab_button(mode_names[i], 88.f, mode_sels[i]);
			menu->addChildAtPosition(btn, Anchor::Center, ccp(-150.f, -6.f + mode_ys[i]));
			m_mode_buttons.push_back(btn);
		}

		m_mode_hint = add_label(m_sidebar, "", 0.34f, ccp(0.f, -92.f), {160, 160, 175});
		m_mode_hint->setAlignment(kCCTextAlignmentCenter);

		// ---- content: inner tab strip ------------------------------------------
		auto* basic_tab_btn = make_tab_button("Basic", 78.f, menu_selector(CircleToolPopup::on_page_basic));
		auto* advanced_tab_btn = make_tab_button("Advanced", 78.f, menu_selector(CircleToolPopup::on_page_advanced));
		menu->addChildAtPosition(basic_tab_btn, Anchor::Center, ccp(68.f - 45.f, -6.f + 112.f));
		menu->addChildAtPosition(advanced_tab_btn, Anchor::Center, ccp(68.f + 45.f, -6.f + 112.f));
		m_page_buttons = {basic_tab_btn, advanced_tab_btn};

		// info button, top-right corner, explains the active mode
		auto info_btn = CCMenuItemSpriteExtra::create(
			NodeFactory<CCSprite>::start(CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png"))
				.setScale(0.7f),
			this, menu_selector(CircleToolPopup::on_context_info)
		);
		menu->addChildAtPosition(info_btn, Anchor::Center, ccp(200.f, 152.f));

		// ================= BASIC TAB =================
		m_basic_tab = CCNode::create();
		m_basic_tab->setContentSize(m_content->getContentSize());
		m_content->addChildAtPosition(m_basic_tab, Anchor::Center);

		add_field(m_basic_tab, "Arc (deg)", ccp(-72.f, 78.f), 76.f, fmt::to_string(m_angle),
			[this](std::string const& str) {
				m_angle = std::max(0.1f, std::fabs(this->parse_finite(str, m_angle)));
				this->update_labels();
			});

		add_field(m_basic_tab, "Step (deg)", ccp(72.f, 78.f), 76.f, fmt::to_string(m_step),
			[this](std::string const& str) {
				m_step = std::max(0.1f, std::fabs(this->parse_finite(str, m_step)));
				this->update_labels();
			});

		add_label(m_basic_tab, "Squish Shape", 0.38f, ccp(0.f, 48.f), {170, 210, 255});
		auto* squish_uniform_btn = make_tab_button("Uniform", 66.f, menu_selector(CircleToolPopup::on_squish_uniform));
		auto* squish_split_btn = make_tab_button("Split X/Y", 66.f, menu_selector(CircleToolPopup::on_squish_split));
		menu->addChildAtPosition(squish_uniform_btn, Anchor::Center, ccp(68.f - 40.f, -6.f + 30.f));
		menu->addChildAtPosition(squish_split_btn, Anchor::Center, ccp(68.f + 40.f, -6.f + 30.f));
		m_squish_buttons = {squish_uniform_btn, squish_split_btn};

		m_squish_uniform_node = CCNode::create();
		m_squish_uniform_node->setContentSize({280.f, 40.f});
		add_field(m_squish_uniform_node, "Squish", ccp(0.f, -10.f), 90.f, fmt::to_string(m_squish),
			[this](std::string const& str) {
				m_squish = this->parse_finite(str, m_squish);
				this->update_labels();
			});
		m_basic_tab->addChildAtPosition(m_squish_uniform_node, Anchor::Center);

		m_squish_split_node = CCNode::create();
		m_squish_split_node->setContentSize({280.f, 40.f});
		add_field(m_squish_split_node, "Squish X", ccp(-72.f, -10.f), 76.f, fmt::to_string(m_squish_x),
			[this](std::string const& str) {
				m_squish_x = this->parse_finite(str, m_squish_x);
				this->update_labels();
			});
		add_field(m_squish_split_node, "Squish Y", ccp(72.f, -10.f), 76.f, fmt::to_string(m_squish_y),
			[this](std::string const& str) {
				m_squish_y = this->parse_finite(str, m_squish_y);
				this->update_labels();
			});
		m_basic_tab->addChildAtPosition(m_squish_split_node, Anchor::Center);

		add_label(m_basic_tab, "Direction", 0.38f, ccp(0.f, -46.f), {170, 210, 255});
		auto* cw_btn = make_tab_button("CW", 52.f, menu_selector(CircleToolPopup::on_direction_cw));
		auto* ccw_btn = make_tab_button("CCW", 52.f, menu_selector(CircleToolPopup::on_direction_ccw));
		menu->addChildAtPosition(cw_btn, Anchor::Center, ccp(68.f - 34.f, -6.f - 68.f));
		menu->addChildAtPosition(ccw_btn, Anchor::Center, ccp(68.f + 34.f, -6.f - 68.f));
		m_direction_buttons = {cw_btn, ccw_btn};

		m_stats_label = CCLabelBMFont::create("Copies: 0\nObjects: 0", "chatFont.fnt");
		m_stats_label->setAlignment(kCCTextAlignmentCenter);
		m_stats_label->setScale(0.5f);
		m_basic_tab->addChildAtPosition(m_stats_label, Anchor::Center, ccp(0.f, -104.f));

		// ================= ADVANCED TAB =================
		m_advanced_tab = CCNode::create();
		m_advanced_tab->setContentSize(m_content->getContentSize());
		m_content->addChildAtPosition(m_advanced_tab, Anchor::Center);

		add_label(m_advanced_tab, "Pivot Offset (from selection center)", 0.36f, ccp(0.f, 100.f), {255, 220, 120});
		this->m_center_x_input = add_field(m_advanced_tab, "Center X", ccp(-72.f, 74.f), 76.f, fmt::to_string(m_center_x),
			[this](std::string const& str) { m_center_x = this->parse_finite(str, m_center_x); });
		this->m_center_y_input = add_field(m_advanced_tab, "Center Y", ccp(72.f, 74.f), 76.f, fmt::to_string(m_center_y),
			[this](std::string const& str) { m_center_y = this->parse_finite(str, m_center_y); });

		add_label(m_advanced_tab, "Mode Parameters", 0.36f, ccp(0.f, 32.f), {255, 220, 120});

		m_spiral_extra = CCNode::create();
		m_spiral_extra->setContentSize({280.f, 40.f});
		add_field(m_spiral_extra, "Turns", ccp(-72.f, 0.f), 76.f, fmt::to_string(m_spiral_turns),
			[this](std::string const& str) {
				m_spiral_turns = std::max(0.05f, std::fabs(this->parse_finite(str, m_spiral_turns)));
			});
		add_field(m_spiral_extra, "Start %", ccp(72.f, 0.f), 76.f, fmt::to_string(m_spiral_start_percent),
			[this](std::string const& str) {
				m_spiral_start_percent = std::clamp(this->parse_finite(str, m_spiral_start_percent), 0.f, 100.f);
			});
		m_advanced_tab->addChildAtPosition(m_spiral_extra, Anchor::Center, ccp(0.f, 5.f));

		m_wave_extra = CCNode::create();
		m_wave_extra->setContentSize({280.f, 40.f});
		add_field(m_wave_extra, "Frequency", ccp(-72.f, 0.f), 76.f, fmt::to_string(m_wave_frequency),
			[this](std::string const& str) {
				m_wave_frequency = std::max(0.f, this->parse_finite(str, m_wave_frequency));
			});
		add_field(m_wave_extra, "Amplitude", ccp(72.f, 0.f), 76.f, fmt::to_string(m_wave_amplitude),
			[this](std::string const& str) {
				m_wave_amplitude = this->parse_finite(str, m_wave_amplitude);
			});
		m_advanced_tab->addChildAtPosition(m_wave_extra, Anchor::Center, ccp(0.f, 5.f));

		m_random_extra = CCNode::create();
		m_random_extra->setContentSize({280.f, 40.f});
		add_field(m_random_extra, "Jitter", ccp(-72.f, 0.f), 76.f, fmt::to_string(m_random_jitter),
			[this](std::string const& str) {
				m_random_jitter = this->parse_finite(str, m_random_jitter);
			});
		auto* seed_input = geode::TextInput::create(76.f, "");
		seed_input->setCommonFilter(CommonFilter::Float);
		seed_input->setString(fmt::to_string(m_random_seed));
		seed_input->setCallback([this](std::string const& str) {
			auto value = geode::utils::numFromString<float>(str).unwrapOr(static_cast<float>(m_random_seed));
			m_random_seed = std::isfinite(value) ? static_cast<int>(value) : m_random_seed;
		});
		add_label(m_random_extra, "Seed", 0.4f, ccp(72.f, 17.f), {170, 210, 255});
		m_random_extra->addChildAtPosition(seed_input, Anchor::Center, ccp(72.f, 0.f));
		m_advanced_tab->addChildAtPosition(m_random_extra, Anchor::Center, ccp(0.f, 5.f));

		m_circle_extra = CCNode::create();
		m_circle_extra->setContentSize({280.f, 40.f});
		add_label(m_circle_extra, "Circle mode has no extra parameters.", 0.38f, ccp(0.f, 0.f), {160, 160, 175});
		m_advanced_tab->addChildAtPosition(m_circle_extra, Anchor::Center, ccp(0.f, 5.f));

		add_label(m_advanced_tab, "Extra parameters change based on the selected Mode.", 0.32f, ccp(0.f, -70.f), {130, 130, 145});

		// ---- bottom buttons -----------------------------------------------------
		const float button_width = 74.f;
		menu->addChildAtPosition(
			CCMenuItemSpriteExtra::create(
				ButtonSprite::create("Apply", button_width, true, "goldFont.fnt", "GJ_button_01.png", 0, 0.75f),
				this, menu_selector(CircleToolPopup::on_apply)
			),
			Anchor::Center, ccp(button_width / 2.f + 90.f, -158.f)
		);
		menu->addChildAtPosition(
			CCMenuItemSpriteExtra::create(
				ButtonSprite::create("Cancel", button_width, true, "goldFont.fnt", "GJ_button_01.png", 0, 0.75f),
				this, menu_selector(CircleToolPopup::onClose)
			),
			Anchor::Center, ccp(button_width / -2.f - 90.f, -158.f)
		);

		// ---- restore persisted UI state ------------------------------------------
		this->refresh_mode_buttons();
		this->refresh_squish_buttons();
		this->refresh_direction_buttons();
		this->refresh_page();
		this->update_labels();

		return true;
	}

	TextInput* m_center_x_input = nullptr;
	TextInput* m_center_y_input = nullptr;

	// ---------------------------------------------------------------------
	// state <-> UI syncing
	// ---------------------------------------------------------------------

	float parse_finite(std::string const& text, float fallback) const {
		const auto value = geode::utils::numFromString<float>(text).unwrapOr(fallback);
		return std::isfinite(value) ? value : fallback;
	}

	size_t copy_count() const {
		const auto arc = std::fabs(m_angle);
		const auto step = std::max(0.1f, m_step);
		return static_cast<size_t>(std::max(0.f, std::ceil(arc / step) - 1.f));
	}

	void update_labels() {
		if (!m_stats_label) return;
		auto* editor = GameManager::sharedState()->getEditorLayer();
		auto* objs = editor ? editor->m_editorUI->getSelectedObjects() : nullptr;
		const auto amount = this->copy_count();
		const auto object_count = objs ? amount * objs->count() : 0;
		m_stats_label->setString(fmt::format("Copies: {}\nObjects: {}", amount, object_count).c_str());
	}

	void refresh_mode_buttons() {
		for (int i = 0; i < static_cast<int>(m_mode_buttons.size()); ++i) {
			set_active_state(m_mode_buttons[i], i == static_cast<int>(m_mode));
		}
		const char* hints[4] = {
			"Duplicates around a\nfull or partial circle.",
			"Duplicates while moving\noutward/inward like a spiral.",
			"Duplicates with a rippling\nsine-wave radius.",
			"Duplicates with randomized\nradius per copy.",
		};
		m_mode_hint->setString(hints[static_cast<int>(m_mode)]);

		m_spiral_extra->setVisible(m_mode == ToolMode::Spiral);
		m_wave_extra->setVisible(m_mode == ToolMode::Wave);
		m_random_extra->setVisible(m_mode == ToolMode::Random);
		m_circle_extra->setVisible(m_mode == ToolMode::Circle);
	}

	void refresh_squish_buttons() {
		for (int i = 0; i < static_cast<int>(m_squish_buttons.size()); ++i) {
			set_active_state(m_squish_buttons[i], (i == 1) == m_split_squish);
		}
		m_squish_uniform_node->setVisible(!m_split_squish);
		m_squish_split_node->setVisible(m_split_squish);
	}

	void refresh_direction_buttons() {
		set_active_state(m_direction_buttons[0], m_direction_cw);
		set_active_state(m_direction_buttons[1], !m_direction_cw);
	}

	void refresh_page() {
		for (int i = 0; i < static_cast<int>(m_page_buttons.size()); ++i) {
			set_active_state(m_page_buttons[i], (i == 1) == m_advanced_tab_active);
		}
		m_basic_tab->setVisible(!m_advanced_tab_active);
		m_advanced_tab->setVisible(m_advanced_tab_active);
	}

	// ---------------------------------------------------------------------
	// callbacks
	// ---------------------------------------------------------------------

	void set_mode(ToolMode mode) {
		m_mode = mode;
		this->refresh_mode_buttons();
		this->update_labels();
	}

	void on_mode_circle(CCObject*) { this->set_mode(ToolMode::Circle); }
	void on_mode_spiral(CCObject*) { this->set_mode(ToolMode::Spiral); }
	void on_mode_wave(CCObject*) { this->set_mode(ToolMode::Wave); }
	void on_mode_random(CCObject*) { this->set_mode(ToolMode::Random); }

	void on_squish_uniform(CCObject*) { m_split_squish = false; this->refresh_squish_buttons(); }
	void on_squish_split(CCObject*) { m_split_squish = true; this->refresh_squish_buttons(); }

	void on_direction_cw(CCObject*) { m_direction_cw = true; this->refresh_direction_buttons(); }
	void on_direction_ccw(CCObject*) { m_direction_cw = false; this->refresh_direction_buttons(); }

	void on_page_basic(CCObject*) { m_advanced_tab_active = false; this->refresh_page(); }
	void on_page_advanced(CCObject*) { m_advanced_tab_active = true; this->refresh_page(); }

	void on_context_info(CCObject*) {
		switch (m_mode) {
			case ToolMode::Spiral:
				FLAlertLayer::create(nullptr, "Spiral Mode",
					"Spiral Mode moves each duplicate farther from (or closer to) the "
					"center while it rotates.\n"
					"<cy>Turns</c> sets how many full loops happen across the Arc.\n"
					"<cy>Start %</c> sets the radius the spiral begins at.\n"
					"Try <cy>Arc 360</c>, <cy>Step 5</c>, and <cy>Turns 1</c> first.",
					"OK", nullptr, 420.f
				)->show();
				break;
			case ToolMode::Wave:
				FLAlertLayer::create(nullptr, "Wave Mode",
					"Wave Mode oscillates the squish radius in a sine wave as copies "
					"are placed around the Arc.\n"
					"<cy>Frequency</c> is how many ripples occur across the Arc.\n"
					"<cy>Amplitude</c> is how strong each ripple is.",
					"OK", nullptr, 400.f
				)->show();
				break;
			case ToolMode::Random:
				FLAlertLayer::create(nullptr, "Random Mode",
					"Random Mode applies a random radius offset to each duplicate.\n"
					"<cy>Jitter</c> controls how strong the randomness is.\n"
					"<cy>Seed</c> makes the randomness reproducible: the same seed "
					"always produces the same layout.",
					"OK", nullptr, 400.f
				)->show();
				break;
			case ToolMode::Circle:
			default:
				FLAlertLayer::create(nullptr, "Circle Mode",
					"Circle Mode repeatedly duplicates and rotates the selection "
					"across the Arc.\n"
					"<cy>Step</c> controls the spacing between copies.\n"
					"Use the Advanced tab to change the rotation pivot.",
					"OK", nullptr, 400.f
				)->show();
				break;
		}
	}

	void on_apply(CCObject*) {
		auto* editor = GameManager::sharedState()->getEditorLayer()->m_editorUI;
		auto objs = editor->getSelectedObjects();
		const auto amount = this->copy_count();
		if (objs && objs->count()) {
			const auto obj_count = objs->count() * amount;
			if (obj_count > 5000) {
				createQuickPopup("Warning",
					fmt::format("This will create <cy>{}</c> objects, are you sure?", obj_count),
					"Cancel", "Ok",
					[this](auto*, bool btn2) {
						if (btn2) {
							this->perform();
						}
					}
				);
			} else {
				perform();
			}
		}
	}

	// ---------------------------------------------------------------------
	// actual duplication logic
	// ---------------------------------------------------------------------

	void perform() {
		auto* editor = GameManager::sharedState()->getEditorLayer();
		auto* editor_ui = editor->m_editorUI;
		auto* objs = CCArray::create();

		const ToolMode mode = m_mode;
		const float step_mag = std::max(0.1f, m_step);
		const float step = m_direction_cw ? step_mag : -step_mag;
		const float arc = std::max(0.1f, std::fabs(m_angle));

		const float squish_x = m_split_squish ? m_squish_x : m_squish;
		const float squish_y = m_split_squish ? m_squish_y : m_squish;

		const float spiral_turns = std::max(0.05f, m_spiral_turns);
		const float spiral_start = std::clamp(m_spiral_start_percent, 0.f, 100.f) / 100.f;
		const float wave_freq = std::max(0.f, m_wave_frequency);
		const float wave_amp = m_wave_amplitude;
		const int random_seed = m_random_seed;
		const float random_jitter = m_random_jitter;

		const CCPoint pivot = {m_center_x, m_center_y};
		constexpr float TAU = 6.28318530f;
		constexpr float DEG2RAD = 0.01745329f;

		const auto radius_factor = [&](float progress, int index) -> float {
			switch (mode) {
				case ToolMode::Spiral:
					return spiral_start + (1.f - spiral_start) * progress;
				case ToolMode::Wave:
					return 1.f + wave_amp * sinf(progress * wave_freq * TAU);
				case ToolMode::Random:
					return 1.f + random_jitter * pseudo_random(random_seed, index);
				case ToolMode::Circle:
				default:
					return 1.f;
			}
		};

		const auto calc = [&](float angle_deg, int index) -> CCPoint {
			const float progress = angle_deg / arc;
			const float path_radians = (mode == ToolMode::Spiral)
				? progress * spiral_turns * TAU
				: angle_deg * DEG2RAD;
			const float radius = radius_factor(progress, index);
			return CCPoint{
				sinf(path_radians) * squish_x * radius,
				cosf(path_radians) * squish_y * radius,
			};
		};

		CCPoint off_acc = calc(0.f, 0);
		int index = 0;
		for (float i = 1; i * step_mag < arc; ++i, ++index) {
			editor_ui->onDuplicate(nullptr);
			auto selected = editor_ui->getSelectedObjects();
			editor_ui->rotateObjects(selected, step, pivot);

			const float angle = i * step_mag;
			const auto offset = calc(angle, index + 1);
			const CCPoint delta = {offset.x - off_acc.x, offset.y - off_acc.y};

			if (delta.x != 0.f || delta.y != 0.f) {
				for (auto obj : CCArrayExt<GameObject*>(selected)) {
					editor_ui->moveObject(obj, delta);
				}
			}

			off_acc = offset;

			// remove undo object for pasting the objs
			editor->m_undoObjects->removeLastObject();
			objs->addObjectsFromArray(selected);
		}
		editor->m_undoObjects->addObject(UndoObject::createWithArray(objs, UndoCommand::Paste));
		// second argument is ignoreSelectFilter
		editor_ui->selectObjects(objs, true);
		this->keyBackClicked();
	}
};

ToolMode CircleToolPopup::m_mode = ToolMode::Circle;
float CircleToolPopup::m_angle = 180.f;
float CircleToolPopup::m_step = 5.f;
bool CircleToolPopup::m_direction_cw = true;

bool CircleToolPopup::m_split_squish = false;
float CircleToolPopup::m_squish = 0.f;
float CircleToolPopup::m_squish_x = 0.f;
float CircleToolPopup::m_squish_y = 0.f;

float CircleToolPopup::m_center_x = 0.f;
float CircleToolPopup::m_center_y = 0.f;

float CircleToolPopup::m_spiral_turns = 1.f;
float CircleToolPopup::m_spiral_start_percent = 0.f;

float CircleToolPopup::m_wave_frequency = 3.f;
float CircleToolPopup::m_wave_amplitude = 0.3f;

float CircleToolPopup::m_random_jitter = 0.15f;
int CircleToolPopup::m_random_seed = 1;

bool CircleToolPopup::m_advanced_tab_active = false;


class $modify(MyEditorUI, EditorUI) {
	void on_circle_tool(CCObject*) {
		if (this->getSelectedObjects()->count()) {
			CircleToolPopup::create()->show();
		} else {
			FLAlertLayer::create("Info", "You must select some objects to use circle tool", "OK")->show();
		}
	}

	void createMoveMenu() {
		EditorUI::createMoveMenu();
		auto* btn = this->getSpriteButton("button.png"_spr, menu_selector(MyEditorUI::on_circle_tool), nullptr, 0.9f);
		m_editButtonBar->m_buttonArray->addObject(btn);
		auto rows = GameManager::sharedState()->getIntGameVariable("0049");
		auto cols = GameManager::sharedState()->getIntGameVariable("0050");
		// TODO: replace with reloadItemsInNormalSize
		m_editButtonBar->reloadItems(rows, cols);
	}
};
