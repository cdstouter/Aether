#include <cmath>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <memory>

#include <pugl/pugl.hpp>

#include "gl_helper.hpp"

#include "style.hpp"

struct Root;
struct DrawingContext;
class Group;

template <class T>
struct ParseResult {
	T val;
	const char* end;

	// to allow the use of std::tie
	template <typename A, typename B>
	operator std::tuple<A, B>() { return {val, end}; }
};

// only supports interpolation between two values of the form "%f unit"
template <class T>
static std::string interpolate_style(
	float t,
	const std::pair<std::string, std::string>& range
) {
	char* num1_end;
	float val1 = std::strtof(range.first.c_str(), &num1_end);
	char* num2_end;
	float val2 = std::strtof(range.second.c_str(), &num2_end);
	T val = static_cast<T>(std::lerp(val1, val2, t));

	return std::to_string(val) + num1_end;
}

class UIElement {
public:

	using ButtonPressCallback = std::function<void (UIElement*, const pugl::ButtonPressEvent&)>;
	using ButtonReleaseCallback = std::function<void (UIElement*, const pugl::ButtonReleaseEvent&)>;
	using MotionCallback = std::function<void (UIElement*, const pugl::MotionEvent&)>;

	struct Connection {
		size_t param_idx;

		std::string style;
		std::pair<float, float> in_range;
		std::pair<std::string, std::string> out_range;

		std::function<std::string (
				float,
				const std::pair<std::string, std::string>&
			)> interpolate = interpolate_style<float>;

		//
		mutable float last_value = std::numeric_limits<float>::quiet_NaN();
	};

	struct CreateInfo {
		bool visible;
		bool inert;
		ButtonPressCallback btn_press_callback = nullptr;
		ButtonReleaseCallback btn_release_callback = nullptr;
		MotionCallback motion_callback = nullptr;
		std::vector<Connection> connections = {};
		std::unordered_map<std::string, std::string> style;
	};

	UIElement(Root* root, Group* parent, CreateInfo create_info) noexcept;
	UIElement(const UIElement& other) = delete;
	virtual ~UIElement() = default;

	UIElement& operator=(const UIElement& other) noexcept = delete;

	/*
		Draws the UI element
	*/
	void draw() const;

	/*
		Updates the connection values
	*/
	void update_connections();

	/*
		checks if the element is interactable then calls element_at_impl.
		returns the topmost interactable element over the point x,y.
		returns nullptr if there are no elements over the point x,y.
	*/
	[[nodiscard]] UIElement* element_at(float x, float y) {
		return m_inert ? nullptr : element_at_impl(x, y);
	}

	/*
		shows/hides the ui element.
	*/
	[[nodiscard]] bool visible() noexcept { return m_visible; }
	void visible(bool b) noexcept { m_visible = b; }

	/*
		makes the ui element inert/interactable.
	*/
	[[nodiscard]] bool inert() noexcept {return m_inert; }
	void inert(bool b) noexcept { m_inert = b; }

	/*
		set callbacks for mouse events
	*/
	void set_btn_press_callback(ButtonPressCallback cb) noexcept { m_btn_prs_cb = cb; }
	void set_btn_release_callback(ButtonReleaseCallback cb) noexcept { m_btn_rls_cb = cb; }
	void set_motion_callback(MotionCallback cb) noexcept { m_motion_cb = cb; }

	/*
		trigger mouse events
	*/
	virtual void btn_press(const pugl::ButtonPressEvent& e) {
		if (m_btn_prs_cb) m_btn_prs_cb(this, e);
	}
	virtual void btn_release(const pugl::ButtonReleaseEvent& e) {
		if (m_btn_rls_cb) m_btn_rls_cb(this, e);
	}
	virtual void motion(const pugl::MotionEvent& e) {
		if (m_motion_cb) m_motion_cb(this, e);
	}

	const Root* root() const { return m_root; }
protected:

	// Variables
	mutable Style style;

	Group* m_parent;
	Root* m_root;

	/*
		attempt to find the value in style with key name
		throws a runtime_error with string err if the key does not exist
	*/
	std::string_view get_style(const std::string& name, std::string err) const;

	/*
		unit conversions
		They are member functions as some units are
		relative to parent dimensions
	*/
	ParseResult<float> to_px(const char* expr) const;
	ParseResult<float> to_horizontal_px(const char* expr) const;
	ParseResult<float> to_vertical_px(const char* expr) const;
	ParseResult<float> to_rad(const char* expr) const;

	/*
		sets the current fill/stroke to be rendered using nvgFill/nvgStroke
		will return false if the element has no fill/stroke to be set
	*/
	bool set_fill() const;
	bool set_stroke() const;

	/*
		draws the ui element
		implemented by the subclass
	*/
	virtual void draw_impl() const = 0;

	/*
		returns the topmost interactable element
		implemented by the subclass
	*/
	virtual UIElement* element_at_impl(float x, float y) = 0;

private:
	std::vector<Connection> param_connections;

	mutable bool m_visible;
	mutable bool m_inert;

	ButtonPressCallback m_btn_prs_cb;
	ButtonReleaseCallback m_btn_rls_cb;
	MotionCallback m_motion_cb;

	/*

	*/
	void apply_transforms() const;

	void set_new_render_target() const;
	void apply_filter(std::string_view filter) const;
};

/*
	Subclasses
*/

class Circle : public UIElement {
public:
	Circle(Root* root, Group* parent, CreateInfo create_info) noexcept :
		UIElement(root, parent, create_info) {}
protected:

	[[nodiscard]] float cx() const;
	[[nodiscard]] float cy() const;
	[[nodiscard]] float r() const;

	/*
		Virtual functions
	*/
	virtual void draw_impl() const override;
	virtual UIElement* element_at_impl(float x, float y) override;
};

class Arc : public Circle {
public:
	Arc(Root* root, Group* parent, CreateInfo create_info) noexcept :
		Circle(root, parent, create_info) {}
protected:

	[[nodiscard]] float a0() const;
	[[nodiscard]] float a1() const;

	/*
		Virtual functions
	*/
	virtual void draw_impl() const override;
	virtual UIElement* element_at_impl(float x, float y) override;
};


class Path : public UIElement {
public:
	Path(Root* root, Group* parent, CreateInfo create_info) noexcept :
		UIElement(root, parent, create_info) {}

	[[nodiscard]] std::string_view path() const;
protected:
	/*
		Virtual functions
	*/
	virtual void draw_impl() const override;
	virtual UIElement* element_at_impl(float, float) override { return nullptr; }
};

class Rect : public UIElement {
public:
	Rect(Root* root, Group* parent, CreateInfo create_info) noexcept :
		UIElement(root, parent, create_info) {}
	/*
		returns the position of the top left corner in pixels
	*/
	[[nodiscard]] float x() const;
	[[nodiscard]] float y() const;
	/*
		returns the elements dimensions in pixels
	*/
	[[nodiscard]] float width() const;
	[[nodiscard]] float height() const;

	[[nodiscard]] std::array<float, 4> bounds() const;
protected:
	/*
		Virtual functions
	*/
	virtual void draw_impl() const override;
	virtual UIElement* element_at_impl(float x, float y) override;
};

class ShaderRect : public Rect {
public:
	struct UniformInfo {
		std::string name;
		size_t param_idx;
	};

	struct CreateInfo {
		Rect::CreateInfo base;
		std::string frag_shader_code;
		std::vector<UniformInfo> uniform_infos;
	};

	ShaderRect(Root* root, Group* parent, CreateInfo create_info) noexcept :
		Rect(root, parent, create_info.base),
		m_frag_shader_code{create_info.frag_shader_code},
		m_uniforms{create_info.uniform_infos}
	{}

protected:

	static constexpr const char* m_vert_shader_code = ""
		"#version 330 core\n"

		"layout(location = 0) in vec2 vertex_pos;"
		"layout(location = 1) in vec2 vertex_uv;"

		"uniform vec2 corner;"
		"uniform vec2 dimensions;"

		"out vec2 position;"

		"void main() {"
		"	position = vertex_uv;"
		"	vec2 normalized = 0.5f*vertex_pos-0.5f;"
		"	gl_Position = vec4(normalized*dimensions + corner, 0, 1);"
		"}";
	std::string m_frag_shader_code;
	mutable Shader m_shader;
	std::vector<UniformInfo> m_uniforms;

	/*
		Virtual functions
	*/
	virtual void draw_impl() const override;
};

class Spectrum : public Rect {
public:
	Spectrum(Root* root, Group* parent, CreateInfo create_info) noexcept :
		Rect(root, parent, create_info)
	{}
protected:

	/*
		Virtual functions
	*/
	virtual void draw_impl() const override;
};

class RoundedRect : public Rect {
public:
	RoundedRect(Root* root, Group* parent, CreateInfo create_info) noexcept :
		Rect(root, parent, create_info) {}

	/*
		corner radius
	*/
	[[nodiscard]] float r() const;
protected:
	/*
		Virtual functions
	*/
	virtual void draw_impl() const override;
};


class Text : public Rect {
public:
	Text(Root* root, Group* parent, CreateInfo create_info) noexcept :
		Rect(root, parent, create_info) {}
protected:

	[[nodiscard]] std::string_view font_face() const;
	[[nodiscard]] std::string_view text() const;

	[[nodiscard]] std::array<float, 4> bounds() const;
	[[nodiscard]] float font_size() const;

	std::optional<float> defined_width() const;

	void set_alignment() const;

	void set_text_styling() const;

	/*
		Virtual functions
	*/
	virtual void draw_impl() const override;
	virtual UIElement* element_at_impl(float x, float y) override;

private:
	std::array<float, 2> render_corner() const;
};




class Group : public Rect {
public:
	Group(Root* root, Group* parent, CreateInfo create_info) noexcept :
		Rect(root, parent, create_info) {}

	/*
		modifiers
	*/
	template<class Subclass> requires std::is_base_of_v<UIElement, Subclass>
	Subclass* add_child(typename Subclass::CreateInfo&& create_info) {
		auto& added = m_children.emplace_back(
			std::make_unique<Subclass>(m_root, this, std::forward<typename Subclass::CreateInfo>(create_info))
		);
		return dynamic_cast<Subclass*>(added.get());
	}

	auto remove_child(std::vector<std::unique_ptr<UIElement>>::const_iterator pos) {
		return m_children.erase(pos);
	}

	const auto& children() const noexcept { return m_children; }
protected:
	/*
		Virtual functions
	*/
	virtual void draw_impl() const override {
		for (const auto& child : m_children)
			child->draw();
	}

	virtual UIElement* element_at_impl(float x, float y) override {
		UIElement* element = Rect::element_at_impl(x, y);
		if (element) {
			for (int i = m_children.size()-1; i >= 0; --i) {
				UIElement* elem = m_children[i]->element_at(x, y);
				if (elem) return elem;
			}
		}

		return element;
	}
private:
	std::vector<std::unique_ptr<UIElement>> m_children;
};




struct Root final : public Group {
	Root(
		uint32_t width,
		uint32_t height,
		std::filesystem::path bundle_path,
		DrawingContext* ctx
	);

	float vh;
	float vw;

	std::filesystem::path bundle_path;

	// frequency magnitudes
	std::array<std::array<float, 2000>, 2> audio = {};
	// 51 parameters + 12 audio peaks
	std::array<float, 63> parameters = {};

	mutable DrawingContext* ctx;

	int get_font(std::string font_face);
};

struct DrawingContext {
	NVGcontext* nvg_ctx;

	Shader filter_passes[2] = {};
	size_t active_framebuffers = 0;
	std::vector<Framebuffer> framebuffers;
	Framebuffer staging_buffer;

	void initialize(uint32_t width, uint32_t height);
	void destroy() noexcept;
private:

	static constexpr const char* filter_pass_1_frag_code = ""
		"#version 330 core\n"

		"in vec2 uv;"
		"out vec4 color;"

		"uniform sampler2D src;"
		"uniform float blur;"

		"void main() {"
		"	vec4 blurred = texture(src, uv);"
		"	float step = 1.f/textureSize(src, 0).x;"
		"	int radius = int(3.f*blur+0.5);"
		"	for (int dx = 1; dx <= radius; dx += 1) {"
		"		float weight = exp(-dx*dx/(2.f*blur*blur));"
		"		blurred += weight*("
		"			texture(src, uv+vec2(dx*step, 0)) +"
		"			texture(src, uv-vec2(dx*step, 0))"
		"		);"
		"	}"
		"	color = blurred;"
		"}";

	static constexpr const char* filter_pass_2_frag_code = ""
		"#version 330 core\n"

		"in vec2 uv;"
		"out vec4 color;"

		"uniform sampler2D src;"
		"uniform float blur;"

		"const float pi = 3.14159265359;"

		"void main() {"
		"	vec4 blurred = texture(src, uv);"
		"	float step = 1.f/textureSize(src, 0).y;"
		"	int radius = int(3.f*blur+0.5);"
		"	for (int dy = 1; dy <= radius; dy += 1) {"
		"		float weight = exp(-dy*dy/(2.f*blur*blur));"
		"		blurred += weight*("
		"			texture(src, uv+vec2(0, dy*step)) +"
		"			texture(src, uv-vec2(0, dy*step))"
		"		);"
		"	}"
		"	color = blurred/(blur*blur*2*pi);"
		"}";

	static constexpr const char* filter_vert_code = ""
		"#version 330 core\n"

		"layout(location = 0) in vec2 vertex_pos;"
		"layout(location = 1) in vec2 vertex_uv;"

		"out vec2 uv;"

		"void main() {"
		"	uv = vertex_uv;"
		"	gl_Position = vec4(vertex_pos, 0, 1);"
		"}";
};

class UITree {
public:
	UITree(uint32_t width, uint32_t height, std::filesystem::path bundle_path);
	~UITree() = default;

	void draw() const;

	[[nodiscard]] const Root& root() const noexcept;
	[[nodiscard]] Root& root() noexcept;

	void update_viewport(size_t width, size_t height);

	void initialize_context();
	void destroy_context() noexcept;

private:
	mutable DrawingContext m_ctx;
	Root m_root;
};
