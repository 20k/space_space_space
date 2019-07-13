#ifndef SHIP_COMPONENTS_HPP_INCLUDED
#define SHIP_COMPONENTS_HPP_INCLUDED

#include <vector>
#include <string>
#include <map>
#include <vec/vec.hpp>
#include <optional>

#include "radar_field.hpp"
#include "material_info.hpp"
#include "default_components.hpp"
#include "script_execution.hpp"
#include "access_permissions.hpp"

///1 size at a power of 1 takes 100s
#define SIZE_TO_TIME 100

#define HEATING_MULTIPLIER 5

namespace sf
{
    struct RenderWindow;
}

namespace drag_drop_info
{
    enum type
    {
        UNIT,
        FRACTIONAL,
    };
}

struct drag_drop_data
{
    size_t id;
    drag_drop_info::type type;
};

namespace component_info
{
    enum does_type
    {
        HP,
        THRUST,
        S_POWER,
        W_POWER,
        SHIELDS,
        WEAPONS,
        SENSORS,
        COMMS,
        ARMOUR,
        //LIFE_SUPPORT,
        RADIATOR,
        //CREW,
        POWER,
        CAPACITOR, ///weapons
        MISSILE_STORE,
        ORE,
        MATERIAL,
        SELF_DESTRUCT,
        MINING,
        MANUFACTURING,
        LASER,
        RADAR,
        TRACTOR,
        COUNT
    };

    static inline std::vector<std::string> dname
    {
        "HP",
        "Thrust",
        "S-Power",
        "W-Power",
        "Shield",
        "Weapons",
        "Sensors",
        "Comms",
        "Armour",
        //"Oxygen",
        "Radiator",
        //"Crew",
        "Power",
        "Capacitor",
        "Missiles",
        "Ore",
        "Material",
        "Self Destruct",
        "Mining",
        "Manufacturing",
        "Laser",
        "Radar",
        "Tractor",
    };

    enum activation_type
    {
        NO_ACTIVATION,
        TOGGLE_ACTIVATION,
        SLIDER_ACTIVATION,
    };
}

namespace component_class
{
    enum class_type
    {
        WEAPON,
        POWER,
        UTILITY,
        CPU,
        SHIELD,
        ENGINE,
        RADAR,
        SENSOR,
        CARGO,
        COUNT
    };

    static inline std::vector<std::string> class_names
    {
        "Weapon",
        "Power",
        "Utility",
        "CPU",
        "Shield",
        "Engine",
        "Radar",
        "Sensor",
        "Cargo",
    };
}

namespace tag_info
{
    enum tag_type
    {
        TAG_NONE,
        TAG_EJECTOR,
        TAG_MISSILE_BEHAVIOUR,
        TAG_WEAPON,
        TAG_FACTORY,
        TAG_EFACTORY,
        TAG_REFINERY,
        TAG_CPU,
    };
}

///migrate held outside as its a dynamic property
///migrate last_use_s outside as its a dynamic property
struct does_fixed : serialisable
{
    double capacity = 0;
    //double held = 0;
    double recharge = 0;
    double recharge_unconditional = 0;

    double time_between_use_s = 0;
    //double last_use_s = 0;

    component_info::does_type type = component_info::COUNT;

    SERIALISE_SIGNATURE(does_fixed);

    does_fixed scale(float scale) const
    {
        does_fixed ret = *this;

        ret.capacity *= scale;
        ret.recharge *= scale;
        ret.recharge_unconditional *= scale;
        //ret.time_between_use_s *= scale;

        return ret;
    }
};

struct does_dynamic : serialisable
{
    double held = 0;
    double last_use_s = 0;

    component_info::does_type type = component_info::COUNT;

    SERIALISE_SIGNATURE(does_dynamic);
};

struct tag : serialisable
{
    tag_info::tag_type type = tag_info::TAG_NONE;

    SERIALISE_SIGNATURE(tag);
};

struct storage_pipe : serialisable, owned
{
    size_t id_1 = 0;
    size_t id_2 = 0;

    ///+ve = id_1 -> id_2
    ///-ve = id_2 -> id_1
    float flow_rate = 0;
    float max_flow_rate = 0;

    bool goes_to_space = false;

    bool solids = true;
    bool fluids = true;

    ///used for transferring non liquids, how much work has been done transferring stuff so far
    float transfer_work = 0;

    ///ui
    bool is_open = false;

    ///uh ok rpcs
    ///how doth implement

    SERIALISE_SIGNATURE(storage_pipe);

    void set_flow_rate(float in)
    {
        if(isnan(in) || isinf(in))
            return;

        flow_rate = in;

        flow_rate = clamp(flow_rate, -max_flow_rate, max_flow_rate);
    }

    DECLARE_FRIENDLY_RPC(set_flow_rate, float);
};

struct ship;

template<typename C, typename T>
void for_each_ship_hackery(const C& c, T t);

template<typename C, typename T>
void for_each_ship_hackery(C& c, T t);

struct component_fixed_properties : serialisable
{
    std::vector<tag> tags;

    bool no_drain_on_full_production = false;
    bool complex_no_drain_on_full_production = false;

    component_class::class_type c_class = component_class::UTILITY;

    std::string subtype;

    component_info::activation_type activation_type = component_info::NO_ACTIVATION;

    bool heat_sink = false;

    bool production_heat_scales = false;
    double max_use_angle = 0;

    void add(component_info::does_type, double amount);
    void add(component_info::does_type, double amount, double cap);
    void add_unconditional(component_info::does_type, double amount);
    void add(tag_info::tag_type);

    void add_on_use(component_info::does_type, double amount, double time_between_use_s);

    void set_heat(double heat);
    void set_heat_scales_by_production(bool status, component_info::does_type primary);

    void set_no_drain_on_full_production();
    void set_complex_no_drain_on_full_production();

    component_info::does_type primary_type = component_info::COUNT;

    float base_volume = 1;

    SERIALISE_SIGNATURE(component_fixed_properties);

    const does_fixed& get_info(component_info::does_type type) const
    {
        for(const does_fixed& fix : d_info)
        {
            if(fix.type == type)
                return fix;
        }

        throw std::runtime_error("No type in get_info");
    }

    const does_fixed& get_activate_info(component_info::does_type type) const
    {
        for(const does_fixed& fix : d_activate_requirements)
        {
            if(fix.type == type)
                return fix;
        }

        throw std::runtime_error("No type in get_activate_info");
    }

    /*const does_fixed& get_activate_requirement(component_type::does_type type)
    {

    }*/

    std::vector<does_fixed> d_info;
    std::vector<does_fixed> d_activate_requirements;

    void set_internal_volume(float ivol)
    {
        internal_volume = ivol;
    }

    float get_internal_volume(float scale) const
    {
        return internal_volume * scale;
    }

    float get_heat_produced_at_full_usage(float scale) const
    {
        return heat_produced_at_full_usage * scale * HEATING_MULTIPLIER;
    }

private:
    float internal_volume = 0;
    double heat_produced_at_full_usage = 0;
};

struct blueprint_manager;
struct blueprint;

struct pending_transfer : serialisable, free_function
{
    size_t pid_ship_from = -1;
    size_t pid_component = -1;
    size_t pid_ship_to = -1;
    float fraction = 1;
    bool is_fractiony = false;
};

inline
std::vector<pending_transfer>& client_pending_transfers()
{
    static thread_local std::vector<pending_transfer> txf;
    return txf;
}

struct build_in_progress;

struct component : serialisable, owned, rate_limited, free_function, smoothed
{
    component_type::type base_id = component_type::COUNT;

    //std::vector<does> info;
    //std::vector<does> activate_requirements;
    //std::vector<tag> tags;

    //bool no_drain_on_full_production = false;
    //bool complex_no_drain_on_full_production = false;

    //static inline uint32_t gid = 0;
    //uint32_t id = gid++;

    std::vector<does_dynamic> dyn_info;
    std::vector<does_dynamic> dyn_activate_requirements;

    //std::vector<pending_transfer> transfers;

    std::string long_name;
    std::string short_name;

    cpu_state cpu_core;
    std::string current_directory;

    ///?????????????????
    ///this will go to the house
    //std::string subtype;

    double last_sat = 1;
    bool flows = false;
    bool cleanup = false;
    bool is_build_holder = false; ///holds resources for a ship build
    //bool heat_sink = false;

    ///0 = solid, 1 = liquid
    int phase = 0;

    ///how active i need to be
    ///only applies to no_drain_on_full_production
    ///has a minimum value to prevent accidental feedback loops
    double last_production_frac = 1;
    ///user requested activation level
    float activation_level = 1;
    bool last_could_use = false;
    bool last_activation_successful = false;
    //component_info::activation_type activation_type = component_info::NO_ACTIVATION;

    float radar_offset_angle = 0;
    float radar_restrict_angle = M_PI;

    access_permissions foreign_access;
    void change_access_permissions(access_permissions::state st);
    void change_docking_permissions(bool allow);
    DECLARE_FRIENDLY_RPC(change_access_permissions, access_permissions::state);
    DECLARE_FRIENDLY_RPC(change_docking_permissions, bool);

    ///time this component has been nearly empty enough to remove
    std::optional<fixed_clock> bad_time;

    bool building = false;
    std::vector<std::shared_ptr<build_in_progress>> build_queue;

    std::vector<size_t> unchecked_blueprints;

    ///does heat scale depending on how much of the output is used?
    ///aka power gen
    //bool production_heat_scales = false;


    const component_fixed_properties& get_fixed_props()
    {
        return get_component_fixed_props(base_id, current_scale);
    }

    void manufacture_blueprint_id(size_t print_id);
    void manufacture_blueprint(const blueprint& print, ship& parent);

    DECLARE_FRIENDLY_RPC(manufacture_blueprint_id, size_t);

    double satisfied_percentage(double dt_s, const std::vector<double>& res);
    void apply(const std::vector<double>& efficiency, double dt_s, std::vector<double>& res);

    bool has(component_info::does_type type)
    {
        for(auto& i : dyn_info)
        {
            if(i.type == type)
                return true;
        }

        return false;
    }

    bool has_tag(tag_info::tag_type tag)
    {
        const component_fixed_properties& fixed = get_component_fixed_props(base_id, current_scale);

        for(auto& i : fixed.tags)
        {
            if(i.type == tag)
                return true;
        }

        return false;
    }

    does_dynamic& get_dynamic(component_info::does_type type)
    {
        if(!has(type))
            throw std::runtime_error("rip get");

        //const component_fixed_properties& fixed = get_component_fixed_props(base_id, current_scale);

        for(int i=0; i < (int)dyn_info.size(); i++)
        {
            if(dyn_info[i].type == type)
                return dyn_info[i];
        }

        throw std::runtime_error("rg2");
    }

    does_fixed scale(const does_fixed& fix) const
    {
        return fix.scale(current_scale);
    }

    does_fixed get_fixed(component_info::does_type type)
    {
        if(!has(type))
            throw std::runtime_error("rip get");

        const component_fixed_properties& fixed = get_component_fixed_props(base_id, current_scale);

        return fixed.get_info(type).scale(current_scale);
    }

    does_fixed get_activate_fixed(component_info::does_type type)
    {
        const component_fixed_properties& fixed = get_component_fixed_props(base_id, current_scale);

        return fixed.get_activate_info(type).scale(current_scale);
    }

    ///ignores anything < 0
    std::vector<double> get_theoretical_produced();
    std::vector<double> get_theoretical_consumed();
    std::vector<double> get_produced();
    ///both > 0 && < 0
    std::vector<double> get_needed();
    std::vector<double> get_capacity();
    std::vector<double> get_held();

    double get_sat(const std::vector<double>& sat);

    void deplete_me(const std::vector<double>& diff, const std::vector<double>& free_storage, const std::vector<double>& used_storage);

    bool can_use(const std::vector<double>& res);
    void use(std::vector<double>& res);
    float get_use_heat();

    bool force_use = false;
    bool try_use = false;
    double use_angle = 0;

    ///my volume is how large we are as a thing
    ///internal volume is how much can be stored within me
    ///internal volume should probably be less than my_volume
    //float my_volume = 0;

    ///no longer have a concept of my_volume, purely based off composition
    //float internal_volume = 0;
    float current_scale = 1;


    std::vector<ship> stored;
    std::vector<material> composition;

    float my_temperature = 0;

    //std::vector<component> drain_amount_from_storage(float amount);

    float drain(float amount);
    ///returns amount transferred (absolute value)
    static float drain_material_from_to(component& c1, component& c2, float amount);
    static float drain_ship_from_to(component& c1, component& c2, float amount);

    void normalise_volume();
    float get_my_volume() const;
    float get_stored_volume() const;

    float get_my_temperature();
    float get_my_contained_heat();
    float get_stored_temperature();
    float get_stored_heat_capacity();

    float get_my_heat_x_volume();
    float get_stored_heat_x_volume();

    float get_internal_volume();

    void add_heat_to_me(float heat);
    void remove_heat_from_me(float heat);
    void add_heat_to_stored(float heat);
    void remove_heat_from_stored(float heat);

    bool can_store(const component& c);
    bool can_store(const ship& s);
    void store(const component& c, bool force_and_fixup = false);
    void store(const ship& s);
    bool is_storage();
    bool should_flow();

    float get_hp_frac();
    float get_operating_efficiency();

    void scale(float size);

    std::optional<ship> remove_first_stored_item();

    ///update this to handle fractions, heat, and compounding existing components
    void add_composition(material_info::material_type type, double volume);
    void add_composition(const material& m);
    void add_composition_ratio(const std::vector<material_info::material_type>& type, const std::vector<double>& volume);
    std::vector<material> remove_composition(float amount);

    void handle_drag_drop(size_t parent_id);
    void render_inline_stats();
    std::string phase_string();
    std::string get_render_long_name();
    void render_inline_ui(size_t parent_id, bool use_title = true, bool drag_drop = true);
    void render_manufacturing_window(size_t parent_id, blueprint_manager& blueprint_manage, ship& parent, std::vector<ship>& nearby_unfinished);

    ///do not network
    ///needs some adjustments to the network, need to fix ownership n stuff
    bool detailed_view_open = false;
    bool factory_view_open = false;

    /*void transfer_stored_from_to(size_t pid_ship_from, size_t pid_component_to);
    void transfer_stored_from_to_frac(size_t pid_ship_from, size_t pid_component_to, float frac);
    DECLARE_FRIENDLY_RPC(transfer_stored_from_to, size_t, size_t);
    DECLARE_FRIENDLY_RPC(transfer_stored_from_to_frac, size_t, size_t, float);*/

    void set_activation_level(double level)
    {
        if(isinf(level) || isnan(level))
            return;

        const component_fixed_properties& fixed = get_component_fixed_props(base_id, current_scale);

        if(fixed.activation_type == component_info::NO_ACTIVATION)
        {
            activation_level = 1;
            return;
        }

        if(fixed.activation_type == component_info::TOGGLE_ACTIVATION)
        {
            if(level < 0.5)
                level = 0;
            if(level >= 0.5)
                level = 1;
        }

        activation_level = clamp(level, 0., 1.);
    }

    void set_use()
    {
        try_use = true;
    }


    template<typename T>
    void for_each_stored(T t)
    {
        return for_each_ship_hackery(*this, t);
    }

    template<typename T>
    void for_each_stored(T t) const
    {
        return for_each_ship_hackery(*this, t);
    }
};

struct data_tracker : serialisable, owned
{
    std::vector<float> vsat;
    std::vector<float> vheld;
    int max_data = 500;

    void add(double sat, double held);

    SERIALISE_SIGNATURE(data_tracker);
};

struct alt_radar_field;
struct client_fire;
struct player_model;
struct persistent_user_data;

struct playspace_manager;
struct playspace;
struct room;

struct ship : heatable_entity, free_function
{
    size_t network_owner = 0;
    float my_size = 0;

    std::string blueprint_name;
    std::shared_ptr<blueprint> original_blueprint;

    std::vector<component> components;
    std::vector<storage_pipe> pipes;

    std::vector<component_info::does_type> tracked
    {
        component_info::S_POWER,
        component_info::W_POWER,
        component_info::SHIELDS,
        component_info::ARMOUR,
        component_info::HP,
        component_info::POWER,
        component_info::CAPACITOR
    };

    //std::map<component_info::does_type, data_tracker> data_track;

    ///so we want a diff wrapper type
    //std::vector<data_tracker> data_track;

    persistent_user_data* persistent_data = nullptr;

    std::vector<data_tracker> data_track;

    float construction_amount = 0;
    std::shared_ptr<alt_radar_field> current_radar_field;

    std::vector<std::string> blueprint_tags;
    std::string current_directory;

    ship();

    int room_type = 0;
    int last_room_type = 0;

    bool move_up = false;
    bool has_s_power = false;
    bool has_w_power = false;
    bool move_down = false;
    bool move_warp = false;
    size_t warp_to_pid = 0;
    size_t current_room_pid = -1;
    size_t destination_poi_pid = -1;
    bool travelling_to_poi = false;
    vec2f destination_poi_position;
    bool travelling_in_realspace = false;
    cpu_move_args move_args;
    //vec2f realspace_destination;
    //size_t realspace_pid_target = -1;

    bool is_build_holder = false;

    void resume_building(size_t factory_component_pid, size_t object_pid);
    void cancel_building(size_t factory_component_pid, size_t object_pid);

    DECLARE_FRIENDLY_RPC(resume_building, size_t, size_t);
    DECLARE_FRIENDLY_RPC(cancel_building, size_t, size_t);

    std::optional<component*> get_component_from_id(uint64_t id);

    void handle_cleanup();

    void handle_heat(double dt_s);
    std::vector<component> handle_degredation(double dt_s);
    void tick(double dt_s) override;
    void tick_pre_phys(double dt_s) override;
    //void render(sf::RenderWindow& win);

    void check_space_rules(double dt_s, playspace_manager& play, playspace* space, room* r);
    void step_cpus(playspace_manager& play, playspace* space, room* r);

    void add(const component& c);

    void set_thrusters_active(double frac);

    std::vector<double> get_sat_percentage();
    std::vector<double> get_net_resources(double dt_s, const std::vector<double>& sat_in);

    std::vector<double> get_capacity();

    std::vector<double> last_sat_percentage;

    std::vector<double> radar_frequency_composition;
    std::vector<float> radar_intensity_composition;

    //std::string show_components();
    void show_resources(bool window = true);
    void show_power();
    void show_manufacturing_windows(blueprint_manager& blueprint_manage, std::vector<ship>& nearby_unfinished);

    bool any_building(size_t ship_id);

    float get_my_volume();

    template<typename T, typename U>
    std::vector<T> sum(U in)
    {
        std::vector<T> ret;

        for(component& c : components)
        {
            auto vec = in(c);

            if(ret.size() < vec.size())
                ret.resize(vec.size());

            for(int i=0; i < (int)vec.size(); i++)
            {
                ret[i] += vec[i];
            }
        }

        if(ret.size() != component_info::COUNT)
        {
            ret.resize(component_info::COUNT);
        }

        return ret;
    }

    void apply_force(vec2f dir);
    void apply_rotation_force(float force);

    void fire(const std::vector<client_fire>& fired);
    void ping();
    void take_damage(double amount, bool internal_damage = false, bool base = true);

    double data_track_elapsed_s = 0;

    void add_pipe(const storage_pipe& p);

    double get_sensor_strength();
    float get_max_angular_thrust();
    float get_max_velocity_thrust();
    float get_mass();
    float get_max_temperature();

    virtual void pre_collide(entity& other) override;
    virtual void on_collide(entity_manager& em, entity& other) override;

    void consume_cpu_transfers(room* r);
    std::optional<ship*> fetch_ship_by_id(size_t pid);
    std::optional<component> fetch_component_by_id(size_t pid);
    std::optional<ship> remove_ship_by_id(size_t pid);
    void add_ship_to_component(ship& s, size_t pid);

    std::optional<ship> split_materially(float split_take_frac);

    void new_network_copy();

    alt_radar_sample last_sample;

    sf::Clock spawn_clock;
    uint64_t spawned_by = -1;

    bool is_ship = false;
    room* my_room = nullptr;

private:
    double thrusters_active = 0;
};

bool consume_transfer(room* r, pending_transfer& xfer, size_t pid_ship_initiating);

template<typename C, typename T>
void for_each_ship_hackery(C& c, T t)
{
    for(ship& ns : c.stored)
    {
        for(component& st : ns.components)
        {
            t(st);
        }
    }
}

template<typename C, typename T>
void for_each_ship_hackery(const C& c, T t)
{
    for(const ship& ns : c.stored)
    {
        for(const component& st : ns.components)
        {
            t(st);
        }
    }
}

struct projectile : heatable_entity
{
    projectile();

    void on_collide(entity_manager& em, entity& other) override;
    void tick(double dt_s) override;
};

struct asteroid : heatable_entity
{
    std::shared_ptr<alt_radar_field> current_radar_field;

    asteroid(std::shared_ptr<alt_radar_field> field);
    void init(float min_rad, float max_rad);

    virtual void tick(double dt_s) override;
};

void tick_radar_data(entity_manager& entities, alt_radar_sample& sample, std::shared_ptr<entity>& ship_proxy);
void render_radar_data(sf::RenderWindow& window, const alt_radar_sample& sample);

#endif // SHIP_COMPONENTS_HPP_INCLUDED
