#pragma once

// #include "io/input_action.h"
// #include "io/input_mapping.h"

namespace GLT {

	class event;
	class key_event;
	class mouse_event;
	class world_layer;
	class input_action;
	class input_mapping;

	class player_controller {	
	public:

		player_controller();
		~player_controller();

		DELETE_COPY_CONSTRUCTOR(player_controller);

		USE_IN_EDITOR void set_world_layer_ref(world_layer* world_layer);
		USE_IN_EDITOR FORCEINLINE const ref<input_mapping> get_input_mapping() const { return m_input_mapping; }

		virtual void init();
		virtual void update(f32 delta_time);
		void update_internal(f32 delta_time);
		void handle_event(event& event);

		template <typename T>
		ref<T> register_mapping() {

			LOG(Trace, "register Input mapping");
			ref<T> mapping = create_ref<T>();
			m_input_mapping = std::static_pointer_cast<input_mapping>(mapping);
			return mapping;
		}

		world_layer* m_world_layer{};
	private:

		ref<input_mapping> m_input_mapping{};

		bool handle_key_events(key_event& event);
		bool handle_mouse_events(mouse_event& event);

	};

}
