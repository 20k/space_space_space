#include <networking/serialisable.hpp>

#include "serialisables.hpp"
#include "script_execution.hpp"
#include "ship_components.hpp"
#include "design_editor.hpp"

void register_value::serialise(serialise_context& ctx, nlohmann::json& data, register_value* other)
{
    DO_SERIALISE(which);
    DO_SERIALISE(help);

    if(which == 0)
        DO_SERIALISE(reg);

    if(which == 1)
        DO_SERIALISE(value);

    if(which == 2)
        DO_SERIALISE(symbol);

    if(which == 3)
        DO_SERIALISE(label);

    if(which == 4)
        DO_SERIALISE(address);

    if(which == 5)
        DO_SERIALISE(reg_address);

    if(which == 6)
        DO_SERIALISE(file_eof);
}

void instruction::serialise(serialise_context& ctx, nlohmann::json& data, instruction* other)
{
    DO_SERIALISE(type);
    DO_SERIALISE(args);
}

void cpu_stash::serialise(serialise_context& ctx, nlohmann::json& data, cpu_stash* other)
{
    DO_SERIALISE(held_file);
    DO_SERIALISE(register_states);
    DO_SERIALISE(pc);
    DO_SERIALISE(called_with);
    DO_SERIALISE(my_argument_names);
}

spair::spair()
{

}

spair::spair(register_value f1, int s) : first(f1), second(s){}

void spair::serialise(serialise_context& ctx, nlohmann::json& data, spair* other)
{
    DO_SERIALISE(first);
    DO_SERIALISE(second);
}

void cpu_xfer::serialise(serialise_context& ctx, nlohmann::json& data, cpu_xfer* other)
{
    DO_SERIALISE(from);
    DO_SERIALISE(to);
    DO_SERIALISE(fraction);
    DO_SERIALISE(is_fractiony);
    DO_SERIALISE(held_file);
}

void cpu_state::serialise(serialise_context& ctx, nlohmann::json& data, cpu_state* other)
{
    DO_SERIALISE(audio);
    DO_SERIALISE(all_stash);
    DO_SERIALISE(files);
    DO_SERIALISE(inst);
    DO_SERIALISE(context);

    DO_SERIALISE(free_running);
    DO_SERIALISE(last_error);
    DO_SERIALISE(ports);
    DO_SERIALISE(xfers);
    DO_SERIALISE(blocking_status);
    DO_SERIALISE(waiting_for_hardware_feedback);
    DO_SERIALISE(saved_program);
    DO_SERIALISE(tx_pending);
    DO_SERIALISE(had_tx_pending);
    DO_SERIALISE(my_move);

    DO_RPC(inc_pc);
    DO_RPC(set_program);
    DO_RPC(reset);
    DO_RPC(run);
    DO_RPC(stop);
}

void cpu_file::serialise(serialise_context& ctx, nlohmann::json& data, cpu_file* other)
{
    DO_SERIALISE(name);
    DO_SERIALISE(data);
    DO_SERIALISE(file_pointer);
    DO_SERIALISE(was_xferred);
    DO_SERIALISE(owner);
    DO_SERIALISE(owner_offset);
    DO_SERIALISE(is_hw);
    DO_SERIALISE(alive);
    DO_SERIALISE(stored_in);
}

SERIALISE_BODY_SIMPLE(cpu_move_args)
{
    DO_SERIALISE(name);
    DO_SERIALISE(id);
    DO_SERIALISE(radius);
    DO_SERIALISE(x);
    DO_SERIALISE(y);
    DO_SERIALISE(angle);
    DO_SERIALISE(lax_distance);
}

/*SERIALISE_BODY_SIMPLE(hardware_request)
{
    DO_SERIALISE(has_request);
    DO_SERIALISE(type);
    DO_SERIALISE(id);
}*/

void cpu_state::inc_pc_rpc()
{
    rpc("inc_pc", *this);
}

void cpu_state::upload_program_rpc(std::string str)
{
    rpc("set_program", *this, str);
}

void cpu_state::reset_rpc()
{
    rpc("reset", *this);
}

void cpu_state::run_rpc()
{
    rpc("run", *this);
}

void cpu_state::stop_rpc()
{
    rpc("stop", *this);
}

DECLARE_SERIALISE_FUNCTION(component)
{
    SERIALISE_SETUP();

    //DO_FSERIALISE(info);
    //DO_FSERIALISE(activate_requirements);
    //DO_FSERIALISE(tags);
    DO_FSERIALISE(dyn_info);
    DO_FSERIALISE(dyn_activate_requirements);
    DO_FSERIALISE(base_id);
    DO_FSERIALISE(long_name);
    DO_FSERIALISE(short_name);
    DO_FSERIALISE(last_sat);
    DO_FSERIALISE(flows);
    DO_FSERIALISE(is_build_holder);
    DO_FSERIALISE(phase);
    //DO_FSERIALISE(heat_sink);
    //DO_FSERIALISE(no_drain_on_full_production);
    //DO_FSERIALISE(complex_no_drain_on_full_production);
    DO_FSERIALISE(last_production_frac);
    //DO_FSERIALISE(max_use_angle);
    //DO_FSERIALISE(subtype);
    //DO_FSERIALISE(production_heat_scales);
    //DO_FSERIALISE(my_volume);
    //DO_FSERIALISE(internal_volume);
    DO_FSERIALISE(current_scale);
    DO_FSERIALISE_RATELIMIT(stored, 0, ratelimits::STAGGER);
    //DO_FSERIALISE(primary_type);
    //DO_FSERIALISE(id);
    DO_FSERIALISE(composition);
    DO_FSERIALISE_SMOOTH(my_temperature, interpolation_mode::SMOOTH);
    DO_FSERIALISE(activation_level);
    DO_FSERIALISE(last_could_use);
    DO_FSERIALISE(last_activation_successful);
    DO_FSERIALISE(building);
    DO_FSERIALISE(build_queue);
    DO_FSERIALISE(radar_offset_angle);
    DO_FSERIALISE(radar_restrict_angle);

    if(me->base_id == component_type::CPU)
    {
        DO_FSERIALISE(cpu_core);
    }

    DO_FSERIALISE(current_directory);

    //DO_FSERIALISE(activation_type);
    DO_FRPC(set_activation_level);
    DO_FRPC(set_use);
    DO_FRPC(manufacture_blueprint_id);
    DO_FRPC(transfer_stored_from_to);
    DO_FRPC(transfer_stored_from_to_frac);
}

DECLARE_SERIALISE_FUNCTION(ship)
{
    SERIALISE_SETUP();

    DO_FSERIALISE(construction_amount);
    DO_FSERIALISE(data_track);
    DO_FSERIALISE(network_owner);
    DO_FSERIALISE(components);
    DO_FSERIALISE(last_sat_percentage);
    DO_FSERIALISE(latent_heat);
    DO_FSERIALISE(pipes);
    DO_FSERIALISE(my_size);
    DO_FSERIALISE(is_ship);
    DO_FSERIALISE(blueprint_name);
    DO_FSERIALISE(has_s_power);
    DO_FSERIALISE(has_w_power);
    DO_FSERIALISE(room_type);
    DO_FSERIALISE(last_room_type);
    DO_FSERIALISE(current_room_pid);
    DO_FSERIALISE(travelling_in_realspace);
    //DO_FSERIALISE(realspace_destination);
    //DO_FSERIALISE(realspace_pid_target);
    DO_FSERIALISE(move_args);
    DO_FSERIALISE(radar_frequency_composition);
    DO_FSERIALISE(radar_intensity_composition);
    DO_FSERIALISE(current_directory);
    DO_FSERIALISE(is_build_holder);
    DO_FSERIALISE(original_blueprint);

    DO_FRPC(resume_building);
    DO_FRPC(cancel_building);
}
