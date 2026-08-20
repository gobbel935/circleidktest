#include <Geode/modify/EditorUI.hpp>
#include <nodes.hpp>
#include <algorithm>
#include <cmath>

using namespace geode;
using namespace cocos2d;
using geode::cocos::CCArrayExt;

class CircleToolPopup : public geode::Popup {
public:
	static float m_angle;
	static float m_step;
	static float m_fat;
	static float m_horizontal_squish;
	static float m_vertical_squish;
	static float m_spiral_turns;
	static bool m_advanced_squash_enabled;
	static bool m_spiral_mode_enabled;

		CCLabelBMFont* m_label = nullptr;
		CCNode* m_basic_squish = nullptr;
		CCNode* m_advanced_squish = nullptr;
		CCNode* m_spiral_controls = nullptr;
		CCMenuItemSpriteExtra* m_circle_mode_button = nullptr;
		CCMenuItemSpriteExtra* m_spiral_mode_button = nullptr;
		CCMenuItemSpriteExtra* m_basic_shape_button = nullptr;
		CCMenuItemSpriteExtra* m_advanced_shape_button = nullptr;
		TextInput* m_spiral_turns_input = nullptr;
		TextInput* m_horizontal_input = nullptr;
		TextInput* m_vertical_input = nullptr;

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

	bool init() override {
		if (!Popup::init(390, 300)) return false;

		this->setTitle("Circle Tool");

		auto* layer = m_mainLayer;
		auto* menu = m_buttonMenu;

		// Keep navigation separate from the active parameter content.
		auto* sidebar = CCNode::create();
		sidebar->setContentSize({92.f, 220.f});
		auto* sidebar_bg = CCLayerColor::create(ccc4(22, 22, 32, 160), 92.f, 220.f);
		sidebar_bg->setAnchorPoint({0.5f, 0.5f});
		sidebar->addChildAtPosition(sidebar_bg, Anchor::Center);
		layer->addChildAtPosition(sidebar, Anchor::Center, ccp(-140.f, 4.f));

		auto* content = CCNode::create();
		content->setContentSize({274.f, 220.f});
		auto* content_bg = CCLayerColor::create(ccc4(15, 15, 24, 120), 274.f, 220.f);
		content_bg->setAnchorPoint({0.5f, 0.5f});
		content->addChildAtPosition(content_bg, Anchor::Center);
		layer->addChildAtPosition(content, Anchor::Center, ccp(45.f, 4.f));

		auto add_label = [](CCNode* parent, const char* text, float scale, CCPoint offset) {
			auto* label = NodeFactory<CCLabelBMFont>::start(text, "goldFont.fnt")
				.setScale(scale)
				.end();
			parent->addChildAtPosition(label, Anchor::Center, offset);
			return label;
		};

		add_label(sidebar, "MODE", 0.44f, ccp(0.f, 94.f));
		add_label(sidebar, "SHAPE", 0.44f, ccp(0.f, -28.f));
		add_label(content, "Parameters", 0.62f, ccp(-8.f, 96.f));

		add_label(content, "Arc", 0.58f, ccp(-68.f, 66.f));
		auto angle_input = geode::TextInput::create(58.f, "");
		angle_input->setCommonFilter(CommonFilter::Float);
		angle_input->setString(fmt::to_string(m_angle));
		angle_input->setCallback([this](std::string const& str) {
			auto value = this->parse_finite(str, m_angle);
			m_angle = std::max(0.1f, std::fabs(value));
			this->update_labels();
		});
		content->addChildAtPosition(angle_input, Anchor::Center, ccp(-68.f, 40.f));

		add_label(content, "Step", 0.58f, ccp(68.f, 66.f));
		auto step_input = geode::TextInput::create(58.f, "");
		step_input->setCommonFilter(CommonFilter::Float);
		step_input->setString(fmt::to_string(m_step));
		step_input->setCallback([this](std::string const& str) {
			auto value = this->parse_finite(str, m_step);
			m_step = std::max(0.1f, std::fabs(value));
			this->update_labels();
		});
		content->addChildAtPosition(step_input, Anchor::Center, ccp(68.f, 40.f));

		m_basic_squish = CCNode::create();
		m_basic_squish->setContentSize({220.f, 48.f});
		add_label(m_basic_squish, "Squish", 0.58f, ccp(-48.f, 0.f));
		auto fat_input = geode::TextInput::create(58.f, "");
		fat_input->setCommonFilter(CommonFilter::Float);
		fat_input->setString(fmt::to_string(m_fat));
		fat_input->setCallback([this](std::string const& str) {
			m_fat = this->parse_finite(str, m_fat);
			this->update_labels();
		});
		m_basic_squish->addChildAtPosition(fat_input, Anchor::Center, ccp(50.f, 0.f));
		content->addChildAtPosition(m_basic_squish, Anchor::Center, ccp(0.f, 4.f));

		m_advanced_squish = CCNode::create();
		m_advanced_squish->setContentSize({250.f, 48.f});
		add_label(m_advanced_squish, "Squash Horizontal", 0.43f, ccp(-61.f, 0.f));
		m_horizontal_input = geode::TextInput::create(58.f, "");
		m_horizontal_input->setCommonFilter(CommonFilter::Float);
		m_horizontal_input->setString(fmt::to_string(m_horizontal_squish));
		m_horizontal_input->setCallback([this](std::string const& str) {
			m_horizontal_squish = this->parse_finite(str, m_horizontal_squish);
			this->update_labels();
		});
		m_advanced_squish->addChildAtPosition(m_horizontal_input, Anchor::Center, ccp(-4.f, -19.f));
		add_label(m_advanced_squish, "Squash Vertical", 0.43f, ccp(61.f, 0.f));
		m_vertical_input = geode::TextInput::create(58.f, "");
		m_vertical_input->setCommonFilter(CommonFilter::Float);
		m_vertical_input->setString(fmt::to_string(m_vertical_squish));
		m_vertical_input->setCallback([this](std::string const& str) {
			m_vertical_squish = this->parse_finite(str, m_vertical_squish);
			this->update_labels();
		});
		m_advanced_squish->addChildAtPosition(m_vertical_input, Anchor::Center, ccp(58.f, -19.f));
		content->addChildAtPosition(m_advanced_squish, Anchor::Center, ccp(0.f, 4.f));

		m_spiral_controls = CCNode::create();
		m_spiral_controls->setContentSize({220.f, 48.f});
		add_label(m_spiral_controls, "Spiral Turns", 0.52f, ccp(-48.f, 0.f));
		m_spiral_turns_input = geode::TextInput::create(58.f, "");
		m_spiral_turns_input->setCommonFilter(CommonFilter::Float);
		m_spiral_turns_input->setString(fmt::to_string(m_spiral_turns));
		m_spiral_turns_input->setCallback([this](std::string const& str) {
			auto value = this->parse_finite(str, m_spiral_turns);
			m_spiral_turns = std::max(0.05f, std::fabs(value));
			this->update_labels();
		});
		m_spiral_controls->addChildAtPosition(m_spiral_turns_input, Anchor::Center, ccp(50.f, 0.f));
		content->addChildAtPosition(m_spiral_controls, Anchor::Center, ccp(0.f, -42.f));

		auto create_side_button = [this](const char* text, SEL_MenuHandler callback) {
			return CCMenuItemSpriteExtra::create(
				ButtonSprite::create(text, 68.f, true, "goldFont.fnt", "GJ_button_01.png", 0, 0.55f),
				this, callback
			);
		};

		m_circle_mode_button = create_side_button("Circle", menu_selector(CircleToolPopup::on_circle_mode));
		m_spiral_mode_button = create_side_button("Spiral", menu_selector(CircleToolPopup::on_spiral_mode));
		m_basic_shape_button = create_side_button("Basic", menu_selector(CircleToolPopup::on_basic_shape));
		m_advanced_shape_button = create_side_button("Advanced", menu_selector(CircleToolPopup::on_advanced_shape));
		menu->addChildAtPosition(m_circle_mode_button, Anchor::Center, ccp(-140.f, 58.f));
		menu->addChildAtPosition(m_spiral_mode_button, Anchor::Center, ccp(-140.f, 20.f));
		menu->addChildAtPosition(m_basic_shape_button, Anchor::Center, ccp(-140.f, -58.f));
		menu->addChildAtPosition(m_advanced_shape_button, Anchor::Center, ccp(-140.f, -96.f));

		this->set_mode(m_spiral_mode_enabled);
		this->set_shape(m_advanced_squash_enabled, false);

		auto info_btn = CCMenuItemSpriteExtra::create(
			NodeFactory<CCSprite>::start(CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png"))
				.setScale(0.65f),
			this, menu_selector(CircleToolPopup::on_context_info)
		);
		menu->addChildAtPosition(info_btn, Anchor::Center, ccp(155.f, 100.f));

		m_label = CCLabelBMFont::create("Copies: 0\nObjects: 0", "chatFont.fnt");
		m_label->setAlignment(kCCTextAlignmentLeft);
		m_label->setScale(0.55f);
		content->addChildAtPosition(m_label, Anchor::Center, ccp(-82.f, -91.f));
		this->update_labels();

		const float button_width = 68.f;
		menu->addChildAtPosition(
			CCMenuItemSpriteExtra::create(
				ButtonSprite::create("Apply", button_width, true, "goldFont.fnt", "GJ_button_01.png", 0, 0.75f),
				this, menu_selector(CircleToolPopup::on_apply)
			),
			Anchor::Center, ccp(button_width / 2.f + 82.f, -130.f)
		);
		menu->addChildAtPosition(
			CCMenuItemSpriteExtra::create(
				ButtonSprite::create("Cancel", button_width, true, "goldFont.fnt", "GJ_button_01.png", 0, 0.75f),
				this, menu_selector(CircleToolPopup::onClose)
			),
			Anchor::Center, ccp(button_width / -2.f - 82.f, -130.f)
		);

		return true;
	}

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
		if (!m_label) return;
		auto* editor = GameManager::sharedState()->getEditorLayer();
		auto* objs = editor ? editor->m_editorUI->getSelectedObjects() : nullptr;
		const auto amount = this->copy_count();
		const auto object_count = objs ? amount * objs->count() : 0;
		m_label->setString(fmt::format("Copies: {}\nObjects: {}", amount, object_count).c_str());
	}

	void set_mode(bool spiral) {
		m_spiral_mode_enabled = spiral;
		m_spiral_controls->setVisible(spiral);
		m_circle_mode_button->setOpacity(spiral ? 150 : 255);
		m_spiral_mode_button->setOpacity(spiral ? 255 : 150);
	}

	void set_shape(bool advanced, bool preserve_value) {
		if (preserve_value && advanced && !m_advanced_squash_enabled) {
			m_vertical_squish = m_fat;
			m_vertical_input->setString(fmt::to_string(m_vertical_squish));
		} else if (preserve_value && !advanced && m_advanced_squash_enabled) {
			m_fat = m_vertical_squish;
		}

		m_advanced_squash_enabled = advanced;
		m_basic_squish->setVisible(!advanced);
		m_advanced_squish->setVisible(advanced);
		m_basic_shape_button->setOpacity(advanced ? 150 : 255);
		m_advanced_shape_button->setOpacity(advanced ? 255 : 150);
	}

	void on_circle_mode(CCObject*) {
		this->set_mode(false);
		this->update_labels();
	}

	void on_spiral_mode(CCObject*) {
		this->set_mode(true);
		this->update_labels();
	}

	void on_basic_shape(CCObject*) {
		this->set_shape(false, true);
		this->update_labels();
	}

	void on_advanced_shape(CCObject*) {
		this->set_shape(true, true);
		this->update_labels();
	}

	void on_context_info(CCObject*) {
		if (m_spiral_mode_enabled) {
			FLAlertLayer::create(nullptr, "Spiral Mode",
				"Spiral Mode moves each duplicate farther from the center while it rotates.\n"
				"<cy>Spiral Turns</c> controls the number of full loops across the Arc.\n"
				"Try <cy>Arc 360</c>, <cy>Step 5</c>, and <cy>Spiral Turns 1</c> first.\n"
				"Use Basic for one Squish value, or Advanced for separate horizontal and vertical control.",
				"OK", nullptr, 420.f
			)->show();
		} else {
			FLAlertLayer::create(nullptr, "Circle Mode",
				"Circle Mode repeatedly duplicates and rotates the selection across the Arc.\n"
				"<cy>Step</c> controls the spacing between copies.\n"
				"Use Basic for one Squish value, or Advanced for separate horizontal and vertical control.",
				"OK", nullptr, 400.f
			)->show();
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

	void perform() {
		auto* editor = GameManager::sharedState()->getEditorLayer();
		auto* editor_ui = editor->m_editorUI;
		auto* objs = CCArray::create();

		const bool spiral_enabled = m_spiral_mode_enabled;
		const float step = std::max(0.1f, m_step);
		const float horizontal_squish = m_advanced_squash_enabled ? m_horizontal_squish : (spiral_enabled ? m_fat : 0.f);
		const float vertical_squish = m_advanced_squash_enabled ? m_vertical_squish : m_fat;
		const float spiral_turns = spiral_enabled ? std::max(0.05f, m_spiral_turns) : 1.f;
		const float arc = std::max(0.1f, std::fabs(m_angle));
		const auto calc = [horizontal_squish, vertical_squish, spiral_turns, spiral_enabled, arc](float angle) {
			const float radians = angle / 180.f * 3.141592f;
			const float progress = angle / arc;
			const float path_radians = spiral_enabled ? progress * spiral_turns * 2.f * 3.141592f : radians;
			const float radius = spiral_enabled ? progress : 1.f;
			return CCPoint{
				sinf(path_radians) * horizontal_squish * radius,
				cosf(path_radians) * vertical_squish * radius,
			};
		};
		CCPoint off_acc = calc(0);
		for (float i = 1; i * step < arc; ++i) {
			editor_ui->onDuplicate(nullptr);
			auto selected = editor_ui->getSelectedObjects();
			editor_ui->rotateObjects(selected, step, {0.f, 0.f});

			const float angle = i * step;
			const auto offset = calc(angle);
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

float CircleToolPopup::m_angle = 180.0f;
float CircleToolPopup::m_step = 5.f;
float CircleToolPopup::m_fat = 0.f;
float CircleToolPopup::m_horizontal_squish = 0.f;
float CircleToolPopup::m_vertical_squish = 0.f;
float CircleToolPopup::m_spiral_turns = 1.f;
bool CircleToolPopup::m_advanced_squash_enabled = false;
bool CircleToolPopup::m_spiral_mode_enabled = false;


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
