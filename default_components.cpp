#include "default_components.hpp"
#include <array>
#include "ship_components.hpp"

component make_default(const std::string& long_name, const std::string& short_name, bool flows = false)
{
    component ret;
    ret.long_name = long_name;
    ret.short_name = short_name;
    ret.flows = flows;

    return ret;
}

std::array<component, component_type::COUNT> get_default_component_map()
{
    std::array<component, component_type::COUNT> ret;

    ret[component_type::THRUSTER] = make_default("Thruster", "THRST");
    ret[component_type::S_DRIVE] = make_default("S-Drive", "S-D");
    ret[component_type::W_DRIVE] = make_default("W-Drive", "W-D");
    ret[component_type::SHIELDS] = make_default("Shields", "SHLD");
    ret[component_type::LASER] = make_default("Laser", "LAS");
    ret[component_type::SENSOR] = make_default("Sensors", "SENS");
    ret[component_type::COMMS] = make_default("Communications", "COMM");
    ret[component_type::ARMOUR] = make_default("Armour", "ARMR");
    ret[component_type::LIFE_SUPPORT] = make_default("Life Support", "LS");
    ret[component_type::RADIATOR] = make_default("Radiator", "RAD");
    ret[component_type::POWER_GENERATOR] = make_default("Power Generator", "PWR");
    ret[component_type::CREW] = make_default("Crew", "CRW");
    ret[component_type::MISSILE_CORE] = make_default("Missile Core", "MCRE");
    ret[component_type::DESTRUCT] = make_default("Self Destruct", "SDST");
    ret[component_type::CARGO_STORAGE] = make_default("Cargo", "CRG");
    ret[component_type::STORAGE_TANK] = make_default("Storage Tank", "STNK");
    ret[component_type::STORAGE_TANK_HS] = make_default("Storage Tank HS", "STNKH");
    ret[component_type::HEAT_BLOCK] = make_default("Heat Block", "HBLK");
    ret[component_type::MATERIAL] = make_default("Material", "MAT", true);
    ret[component_type::COMPONENT_LAUNCHER] = make_default("Component Launcher", "CLNCH");
    ret[component_type::MINING_LASER] = make_default("Mining Laser", "MLAS");
    ret[component_type::REFINERY] = make_default("Refinery", "REF");
    ret[component_type::FACTORY] = make_default("Factory", "FAC");
    ret[component_type::RADAR] = make_default("Radar", "RAD");
    ret[component_type::CPU] = make_default("CPU", "CPU");

    for(int i=0; i < component_type::COUNT; i++)
    {
        const component_fixed_properties& fixed = get_component_fixed_props((component_type::type)i, 1.f);

        ret[i].base_id = (component_type::type)i;
        ret[i].dyn_info.resize(fixed.d_info.size());
        ret[i].dyn_activate_requirements.resize(fixed.d_activate_requirements.size());

        for(int kk=0; kk < (int)ret[i].dyn_info.size(); kk++)
        {
            ret[i].dyn_info[kk].held = fixed.d_info[kk].capacity;
            ret[i].dyn_info[kk].type = fixed.d_info[kk].type;
        }

        //ret[i].add_composition(material_info::IRON, fixed.base_volume);
    }

    ret[component_type::THRUSTER].add_composition_ratio({material_info::COPPER}, {1});
    ret[component_type::S_DRIVE].add_composition_ratio({material_info::COPPER}, {1});
    ret[component_type::W_DRIVE].add_composition_ratio({material_info::COPPER}, {1});
    ret[component_type::SHIELDS].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::LASER].add_composition_ratio({material_info::COPPER}, {1});
    ret[component_type::SENSOR].add_composition_ratio({material_info::COPPER}, {1});
    ret[component_type::COMMS].add_composition_ratio({material_info::COPPER}, {1});
    ret[component_type::ARMOUR].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::LIFE_SUPPORT].add_composition_ratio({material_info::COPPER}, {1});
    ret[component_type::RADIATOR].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::POWER_GENERATOR].add_composition_ratio({material_info::COPPER}, {1});
    ret[component_type::CREW].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::MISSILE_CORE].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::DESTRUCT].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::CARGO_STORAGE].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::STORAGE_TANK].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::STORAGE_TANK_HS].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::HEAT_BLOCK].add_composition_ratio({material_info::LITHIUM}, {1});
    ret[component_type::COMPONENT_LAUNCHER].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::MINING_LASER].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::REFINERY].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::FACTORY].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::RADAR].add_composition_ratio({material_info::IRON}, {1});
    ret[component_type::CPU].add_composition_ratio({material_info::IRON}, {1});

    ret[component_type::REFINERY].activation_level = 0;

    return ret;
}

std::array<component_fixed_properties, component_type::COUNT> get_default_fixed_component_map()
{
    std::array<component_fixed_properties, component_type::COUNT> ret;

    {
        component_fixed_properties& p = ret[component_type::THRUSTER];
        p.add(component_info::HP, 0, 1);
        p.add(component_info::POWER, -1);
        p.add(component_info::THRUST, 15);
        p.set_heat(1);
        p.activation_type = component_info::SLIDER_ACTIVATION;
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::S_DRIVE];
        p.add(component_info::HP, 0, 1);
        p.add(component_info::POWER, -5); ///power consumption of 5 modules when charging
        p.add(component_info::S_POWER, 0.05, 1); ///20s to recharge
        p.add_unconditional(component_info::S_POWER, -0.1); ///10s to deplete
        p.add(component_info::S_POWER, 0.1);
        //p.set_no_drain_on_full_production();
        p.set_heat(5); ///heat of 5 modules
        p.activation_type = component_info::TOGGLE_ACTIVATION;
        p.base_volume = 1.5;
    }

    {
        component_fixed_properties& p = ret[component_type::W_DRIVE];
        p.add(component_info::HP, 0, 1);
        p.add(component_info::POWER, -5); ///power consumption of 5 modules when charging
        p.add(component_info::W_POWER, 0.05, 1); ///20s to recharge
        p.add_unconditional(component_info::W_POWER, -0.1); ///10s to deplete
        p.add(component_info::W_POWER, 0.1);
        p.add_on_use(component_info::W_POWER, -1, 5);
        //p.set_no_drain_on_full_production();
        p.set_heat(5); ///heat of 5 modules
        p.activation_type = component_info::TOGGLE_ACTIVATION;
        p.base_volume = 1.5;
    }

    {
        component_fixed_properties& p = ret[component_type::SHIELDS];
        p.add(component_info::HP, 0, 1);
        p.add(component_info::SHIELDS, 0.05, 1); ///20s to recharge
        p.add(component_info::SHIELDS, 0.1);
        p.add_unconditional(component_info::SHIELDS, -0.1);
        p.add(component_info::POWER, -5); ///power consumption of 5 modules when charging
        //p.set_no_drain_on_full_production();
        p.set_heat(3); ///heat of 3 modules
        p.activation_type = component_info::SLIDER_ACTIVATION;
        p.base_volume = 1.1;
    }

    {
        component_fixed_properties& p = ret[component_type::LASER];

        p.add(component_info::POWER, -1);
        p.add(component_info::WEAPONS, 1);
        p.add(component_info::HP, 0, 1);
        p.add(component_info::CAPACITOR, 0.1, 1); ///10s
        p.add(tag_info::TAG_WEAPON);

        p.set_no_drain_on_full_production();
        p.set_heat(5);
        p.activation_type = component_info::TOGGLE_ACTIVATION;

        p.add_on_use(component_info::CAPACITOR, -0.5, 1);
        p.add_on_use(component_info::LASER, 1, 1);
        p.subtype = "laser";

        p.max_use_angle = M_PI/8;
        p.base_volume = 0.8;
    }

    {
        component_fixed_properties& p = ret[component_type::SENSOR];
        p.add(component_info::POWER, -1);
        p.add(component_info::SENSORS, 1);
        p.add(component_info::HP, 0, 1);

        p.add_on_use(component_info::POWER, -5, 1);

        p.set_heat(1);
        p.activation_type = component_info::TOGGLE_ACTIVATION;
        p.base_volume = 0.5;
    }

    {
        component_fixed_properties& p = ret[component_type::COMMS];

        p.add(component_info::POWER, -1);
        p.add(component_info::COMMS, 1);
        p.add(component_info::HP, 0, 1);
        p.set_heat(1);
        p.activation_type = component_info::TOGGLE_ACTIVATION;
        p.base_volume = 0.5;
    }

    {
        component_fixed_properties& p = ret[component_type::ARMOUR];

        p.add(component_info::ARMOUR, 0.01, 5);
        p.add(component_info::POWER, -0.2);
        p.add(component_info::HP, 0, 1);
        p.set_no_drain_on_full_production();
        p.set_heat(0.2); ///5 armour modules emit as much heat as 1 regular
        p.activation_type = component_info::TOGGLE_ACTIVATION;
        p.base_volume = 5;
    }

    {
        component_fixed_properties& p = ret[component_type::LIFE_SUPPORT];

        p.add(component_info::POWER, -1);
        p.add(component_info::LIFE_SUPPORT, 0.1, 10);
        p.add(component_info::HP, 0.01, 1);
        //p.set_no_drain_on_full_production();
        p.set_heat(2);
        p.activation_type = component_info::TOGGLE_ACTIVATION;
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::RADIATOR];

        p.add(component_info::RADIATOR, 1);
        p.add(component_info::HP, 0, 1);
        p.activation_type = component_info::TOGGLE_ACTIVATION;
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::POWER_GENERATOR];

        p.add(component_info::POWER, 10, 50); ///powers 10 standard modules
        p.add(component_info::HP, 0, 1);
        p.set_heat(POWER_TO_HEAT * 10 * 1.5); ///produces the heat of 10 standard modules, with a 50% inefficiency

        p.set_complex_no_drain_on_full_production();
        p.activation_type = component_info::SLIDER_ACTIVATION;
        p.base_volume = 1.5;
    }

    {
        component_fixed_properties& p = ret[component_type::CREW];

        p.add(component_info::HP, 0.1, 1);
        p.add(component_info::LIFE_SUPPORT, -0.05, 1);
        p.add(component_info::COMMS, 0.1);
        p.add(component_info::CREW, 0.01, 1);
        p.add(component_info::CREW, -0.01); ///passive death on no o2, doesn't work yet
        p.set_heat(1);
        p.activation_type = component_info::NO_ACTIVATION;
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::MISSILE_CORE];

        p.add(component_info::HP, 0.0, 1);
        ///hacky until we have the concept of control instead
        p.add(component_info::CREW, 0.01, 1);
        p.add(component_info::CREW, -0.01);
        p.add(component_info::POWER, -1);
        p.add(tag_info::TAG_MISSILE_BEHAVIOUR);
        p.set_heat(1);
        p.activation_type = component_info::NO_ACTIVATION;
        p.base_volume = 0.5;
    }

    {
        component_fixed_properties& p = ret[component_type::DESTRUCT];

        p.add(component_info::HP, 0, 1);
        p.add(component_info::SELF_DESTRUCT, 1);
        //p.set_internal_volume(1);
        p.activation_type = component_info::NO_ACTIVATION;
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::MATERIAL];

        p.add(component_info::HP, 1);
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::CARGO_STORAGE];

        p.add(component_info::HP, 0, 1);
        p.set_internal_volume(1);
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::STORAGE_TANK];

        p.add(component_info::HP, 0, 1);
        p.set_internal_volume(1);
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::STORAGE_TANK_HS];

        p.add(component_info::HP, 0, 1);
        p.set_internal_volume(0.8);
        p.heat_sink = true;
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::HEAT_BLOCK];

        p.activation_type = component_info::NO_ACTIVATION;
        p.add(component_info::HP, 0, 1);
        p.heat_sink = true;
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::COMPONENT_LAUNCHER];

        p.add(component_info::POWER, -1);
        p.add(component_info::HP, 0, 1);
        p.add(tag_info::TAG_EJECTOR);
        p.add(tag_info::TAG_WEAPON);
        p.add_on_use(component_info::POWER, -0.1, 1);
        p.set_no_drain_on_full_production();
        p.set_heat(1);
        p.activation_type = component_info::TOGGLE_ACTIVATION;

        p.set_internal_volume(1);
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::MINING_LASER];

        p.add(component_info::POWER, -1);
        p.add(component_info::HP, 0, 1);
        p.add(tag_info::TAG_WEAPON);
        p.set_heat(1);
        p.activation_type = component_info::TOGGLE_ACTIVATION;

        p.add_on_use(component_info::POWER, -1, 0);
        p.add_on_use(component_info::MINING, 0.001, 0);

        //p.set_internal_volume(1);
        p.base_volume = 1;

        p.subtype = "mining";
    }

    {
        component_fixed_properties& p = ret[component_type::REFINERY];

        p.add(component_info::HP, 0, 1);
        p.add(component_info::POWER, -20); ///2 power plants to run on full tilt
        p.add(tag_info::TAG_REFINERY);
        //p.set_no_drain_on_full_production();
        p.set_heat(40);
        p.activation_type = component_info::SLIDER_ACTIVATION;

        p.base_volume = 1;
        p.set_internal_volume(1);
    }

    {
        component_fixed_properties& p  = ret[component_type::FACTORY];

        p.add(component_info::HP, 0, 1);
        p.add(component_info::POWER, -10); ///1 power plant to run on full tilt
        p.add(component_info::MANUFACTURING, 1);
        p.add(tag_info::TAG_FACTORY);
        p.set_heat(10);

        p.activation_type = component_info::SLIDER_ACTIVATION;

        p.base_volume = 1;
        //p.set_internal_volume(1);
    }

    {
        component_fixed_properties& p = ret[component_type::RADAR];
        p.add(component_info::POWER, -1);
        p.add(component_info::RADAR, 1);
        p.add(component_info::HP, 0, 1);

        p.set_heat(5);
        p.activation_type = component_info::SLIDER_ACTIVATION;
        p.base_volume = 1;
    }

    {
        component_fixed_properties& p = ret[component_type::CPU];
        p.add(component_info::POWER, -1);
        p.add(component_info::HP, 0, 1);
        p.add(tag_info::TAG_CPU);

        p.set_heat(1);
        p.activation_type = component_info::TOGGLE_ACTIVATION;
        p.base_volume = 1;
    }

    return ret;
}

component get_component_default(component_type::type id, float scale)
{
    static std::array<component, component_type::COUNT> built = get_default_component_map();

    component to_fetch = built[id];

    to_fetch._pid = get_next_persistent_id();

    return to_fetch;
}

const component_fixed_properties& get_component_fixed_props(component_type::type id, float scale)
{
    static std::array<component_fixed_properties, component_type::COUNT> built = get_default_fixed_component_map();

    return built[id];
}
