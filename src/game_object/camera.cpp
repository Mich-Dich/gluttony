
#include "util/pch.h"

#include <limits>

#include "camera.h"

namespace GLT {


	camera::camera() { LOG_INIT(); }

	camera::~camera() { LOG_SHUTDOWN(); }

	void camera::set_clipping_dist(const f32 near_dis, const f32 far_dis) {

		PROFILE_FUNCTION();
		
		m_clipping_near = near_dis;
		m_clipping_far = far_dis;
	}

	void camera::set_view_direction(glm::vec3 position, glm::vec3 direction, glm::vec3 up) {

		m_position = position;
		m_direction = direction;

		const glm::vec3 w{ glm::normalize(direction) };
		const glm::vec3 u{ glm::normalize(glm::cross(w, up)) };
		const glm::vec3 v{ glm::cross(w, u) };
		m_view_matrix = glm::mat4{ 1.f };
		m_view_matrix[0][0] = u.x;
		m_view_matrix[1][0] = u.y;
		m_view_matrix[2][0] = u.z;
		m_view_matrix[0][1] = v.x;
		m_view_matrix[1][1] = v.y;
		m_view_matrix[2][1] = v.z;
		m_view_matrix[0][2] = w.x;
		m_view_matrix[1][2] = w.y;
		m_view_matrix[2][2] = w.z;
		m_view_matrix[3][0] = -glm::dot(u, position);
		m_view_matrix[3][1] = -glm::dot(v, position);
		m_view_matrix[3][2] = -glm::dot(w, position);
	}

	void camera::set_view_target(glm::vec3 position, glm::vec3 target, glm::vec3 up) {

		PROFILE_FUNCTION();

		VALIDATE((target - position) != glm::vec3(0.0f, 0.0f, 0.0f), return, "", "Provided position and target are identical")
		set_view_direction(position, target - position, up);
	}

	void camera::set_view_YXZ(glm::vec3 position, glm::vec3 rotation) {

		m_position = position;
		m_direction = rotation;

		const float c1 = glm::cos(rotation.y);
		const float c2 = glm::cos(rotation.x);
		const float c3 = glm::cos(rotation.z);
		const float s1 = glm::sin(rotation.y);
		const float s2 = glm::sin(rotation.x);
		const float s3 = glm::sin(rotation.z);

		const glm::vec3 u{
			c1 * c3 + s1 * s2 * s3,
			c2 * s3,
			c1 * s2 * s3 - c3 * s1
		};
		const glm::vec3 v{
			c3 * s1 * s2 - c1 * s3,
			c2 * c3,
			c1 * c3 * s2 + s1 * s3
		};
		const glm::vec3 w{
			c2 * s1,
			-s2,
			c1 * c2
		};

		m_view_matrix = glm::mat4{ 1.f };
		m_view_matrix[0][0] = u.x;
		m_view_matrix[1][0] = u.y;
		m_view_matrix[2][0] = u.z;
		m_view_matrix[0][1] = v.x;
		m_view_matrix[1][1] = v.y;
		m_view_matrix[2][1] = v.z;
		m_view_matrix[0][2] = w.x;
		m_view_matrix[1][2] = w.y;
		m_view_matrix[2][2] = w.z;
		m_view_matrix[3][0] = -glm::dot(u, position);
		m_view_matrix[3][1] = -glm::dot(v, position);
		m_view_matrix[3][2] = -glm::dot(w, position);
	}

	void camera::set_view_XYZ(glm::vec3 position, glm::vec3 rotation) {

		m_position = position;
		m_direction = rotation;

		const float c1 = glm::cos(rotation.x);
		const float c2 = glm::cos(rotation.y);
		const float c3 = glm::cos(rotation.z);
		const float s1 = glm::sin(rotation.x);
		const float s2 = glm::sin(rotation.y);
		const float s3 = glm::sin(rotation.z);
		const glm::vec3 u{ (c2 * c3), (-c2 * s3), (s2) };
		const glm::vec3 v{ (c1 * s3 + c3 * s1 * s2), (c3 * c1 - s1 * s2 * s3), (-c2 * s1) };
		const glm::vec3 w{ (s1 * s3 - c1 * c3 * s2), (c1 * s2 * s3 + c3 * s1), (c1 * c2) };

		m_view_matrix = glm::mat4{ 1.f };
		m_view_matrix[0][0] = u.x;
		m_view_matrix[1][0] = u.y;
		m_view_matrix[2][0] = u.z;
		m_view_matrix[0][1] = v.x;
		m_view_matrix[1][1] = v.y;
		m_view_matrix[2][1] = v.z;
		m_view_matrix[0][2] = w.x;
		m_view_matrix[1][2] = w.y;
		m_view_matrix[2][2] = w.z;
		m_view_matrix[3][0] = -glm::dot(u, position);
		m_view_matrix[3][1] = -glm::dot(v, position);
		m_view_matrix[3][2] = -glm::dot(w, position);
	}

	void camera::set_orthographic_projection(const f32 left, const  f32 right, const f32 top, const f32 bottom, const f32 near, const f32 far) {

		PROFILE_FUNCTION();

		m_clipping_near = near;
		m_clipping_far = far;

		m_projection_matrix= glm::mat4{ 1.0f };
		m_projection_matrix[0][0] = 2.f / (right - left);
		m_projection_matrix[1][1] = 2.f / (bottom - top);
		m_projection_matrix[2][2] = 1.f / (far - near);
		m_projection_matrix[3][0] = -(right + left) / (right - left);
		m_projection_matrix[3][1] = -(bottom + top) / (bottom - top);
		m_projection_matrix[3][2] = -near / (far - near);
	}

	void camera::set_perspective_projection(const f32 fov_y, const f32 aspect_ratio, const f32 near, const f32 far) {

		PROFILE_FUNCTION();

		ASSERT(glm::abs(aspect_ratio - std::numeric_limits<float>::epsilon()) > 0.0f, "", "Aspect ratio check failed: [aspect_ratio - epsilon] > 0.0f");
		const float tanHalfFovy = tan(fov_y / 2.f);
		m_projection_matrix= glm::mat4{ 0.0f };
		m_projection_matrix[0][0] = 1.f / (aspect_ratio * tanHalfFovy);
		m_projection_matrix[1][1] = 1.f / (tanHalfFovy);
		m_projection_matrix[2][2] = far / (far - near);
		m_projection_matrix[2][3] = 1.f;
		m_projection_matrix[3][2] = -(far * near) / (far - near);

		m_perspective_fov_y = fov_y;
		m_perspective_aspect_ratio = aspect_ratio;
		m_clipping_near = near;
		m_clipping_far = far;
	}

	void camera::set_aspect_ratio(f32 aspect_ratio) {

		PROFILE_FUNCTION();

		m_perspective_aspect_ratio = aspect_ratio;
		update_perspective_projection();
	}

	void camera::set_fov_y(f32 fov_y) {

		PROFILE_FUNCTION();

		m_perspective_fov_y = fov_y;
		update_perspective_projection();
	}

	void camera::auto_calc_fov(const glm::vec2 image_size) {
			
		const float desired_horizontal_fov_deg = 100.0f;
		float hfov_rad = glm::radians(desired_horizontal_fov_deg);
		float aspect_ratio = image_size.x / image_size.y;
		float vfov_rad = 2.0f * atan(tan(hfov_rad / 2.0f) * (1.0f / aspect_ratio));
		LOG(Trace, "Recalculated FOV: " << m_perspective_fov_y)
		m_perspective_fov_y = glm::degrees(vfov_rad);
	}

}
