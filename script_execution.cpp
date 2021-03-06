#include "script_execution.hpp"
#include <assert.h>
#include <iostream>
#include <set>

#include <sstream>
#include <string>
#include "time.hpp"
#include <vec/vec.hpp>
#include "ship_components.hpp"
#include "playspace.hpp"

void enforce_is_ship(cpu_file& fle, size_t ship_pid, size_t root_pid)
{
    fle.is_ship = true;
    fle.is_component = false;
    fle.ship_pid = ship_pid;
    fle.root_ship_pid = root_pid;
}

void enforce_is_component(cpu_file& fle, size_t in_ship_pid, size_t component_pid, size_t root_pid)
{
    fle.is_ship = false;
    fle.is_component = true;
    fle.ship_pid = in_ship_pid;
    fle.component_pid = component_pid;
    fle.root_ship_pid = root_pid;
}

void dump_radar_data_into_cpu(cpu_state& cpu, ship& s, playspace_manager& play, playspace* space, room* r)
{
    int bands = 128;

    std::vector<int> rdata;
    rdata.resize(bands);

    alt_radar_sample& sam = s.last_sample;

    std::optional<cpu_file*> opt_fle = cpu.get_create_capability_file("RADAR_DATA", s._pid, 1, true);

    if(!opt_fle.has_value())
        return;

    cpu_file& fle = *opt_fle.value();

    int max_radar_data = 128;
    int max_write_radar_data = 128;
    int max_connected_systems = 127;
    int max_pois = 127;
    int max_objects = 255;

    float intensity_mult = 16;

    fle.data.resize(max_radar_data + max_write_radar_data + max_connected_systems + max_pois + max_objects);
    fle.ensure_eof();
    cpu.update_length_register();

    for(int i=0; i < (int)fle.len(); i++)
    {
        if(i >= max_radar_data && i < max_radar_data + max_write_radar_data)
            continue;

        fle.data[i].set_int(0);
    }

    int base = 0;

    for(int i=0; i < (int)sam.frequencies.size(); i++)
    {
        float freq = round((sam.frequencies[i] - MIN_FREQ) / (MAX_FREQ - MIN_FREQ));
        float intens = round(sam.intensities[i] * intensity_mult);

        int ifreq = (int)freq;

        ifreq = clamp(ifreq, 0, 127);
        intens = clamp(intens, 0, 256 * intensity_mult - 1);

        fle.data[base + ifreq].value += (int)intens;
        fle.data[base + ifreq].help = "Intensity";
    }

    base += max_radar_data;

    alt_frequency_packet to_send;

    for(int i=0; i < (int)max_write_radar_data; i++)
    {
        if(fle.data[base + i].is_int() && fle.data[base + i].value > 0)
        {
            float intensity_to_send = clamp(fle.data[base + i].value / intensity_mult, 0, 256) / 256.f;
            float frequency = (((float)i / max_write_radar_data) * (MAX_FREQ - MIN_FREQ)) + MIN_FREQ;

            ///so intensity between 0 and 1
            to_send.make(intensity_to_send, frequency);
        }

        fle.data[base + i].help = "Hardware Mapped IO for Frequecy bucket " + std::to_string(i);

        ///if enabled, this makes the radar pulse instead of activate continuously
        fle.data[base + i].set_int(0);
    }

    {
        if(to_send.intensities.size() > 0 && to_send.summed_intensity > 0.001)
        {
            ///sum of intensities should at most be one
            if(to_send.summed_intensity > 1)
            {
                float scale_frac = 1 / to_send.summed_intensity;

                to_send.scale_by(scale_frac);
            }

            s.radar_frequency_composition = to_send.frequencies;
            s.radar_intensity_composition = to_send.intensities;
        }
    }

    base += max_write_radar_data;

    std::vector<playspace*> connected = play.get_connected_systems_for(s.shared_from_this());

    fle.data[base].set_int(connected.size());
    fle.data[base].help = "Number of Connected Systems";

    base++;

    for(int i=0; i < (int)connected.size() && i < max_connected_systems/2; i++)
    {
        size_t pid = connected[i]->_pid;
        std::string name = connected[i]->name;

        fle.data[base + i*2 + 0].set_int(pid);
        fle.data[base + i*2 + 1].set_symbol(name);

        fle.data[base + i*2 + 0].help = "ID";
        fle.data[base + i*2 + 1].help = "Name";
    }

    base += max_connected_systems;

    std::vector<room*> pois = space->all_rooms();

    fle.data[base].set_int(pois.size());
    fle.data[base].help = "Number of POIs";

    base++;

    for(int i=0; i < (int)pois.size() && i < max_pois/3; i++)
    {
        size_t pid = pois[i]->_pid;
        std::string type = poi_type::rnames[(int)pois[i]->ptype];
        //std::string name = pois[i]->name;
        int offset = pois[i]->poi_offset;

        fle.data[base + i * 3 + 0].set_int(pid);
        fle.data[base + i * 3 + 1].set_symbol(type);
        fle.data[base + i * 3 + 2].set_int(offset);

        fle.data[base + i * 3 + 0].help = "ID";
        fle.data[base + i * 3 + 1].help = "Type";
        fle.data[base + i * 3 + 2].help = "Offset";
    }

    base += max_pois;

    fle.data[base].set_int(s.last_sample.renderables.size());
    fle.data[base].help = "Number of Local Objects";

    base++;

    for(int i=0; i < (int)s.last_sample.renderables.size() && i < max_objects/4; i++)
    {
        size_t pid = s.last_sample.renderables[i].uid;
        common_renderable& comm = s.last_sample.renderables[i].property;
        client_renderable& cren = comm.r;

        int x = round(cren.position.x());
        int y = round(cren.position.y());

        int idist = (int)(cren.position - s.r.position).length();
        float angle = round(r2d((cren.position - s.r.position).angle()));

        int items = 6;

        fle.data[base + i * items + 0].set_int(pid);
        fle.data[base + i * items + 1].set_int(x);
        fle.data[base + i * items + 2].set_int(y);
        fle.data[base + i * items + 3].set_int(idist);
        fle.data[base + i * items + 4].set_int(angle);
        fle.data[base + i * items + 5].set_int(round(s.last_sample.renderables[i].summed_intensities * intensity_mult));

        fle.data[base + i * items + 0].help = "ID";
        fle.data[base + i * items + 1].help = "x";
        fle.data[base + i * items + 2].help = "y";
        fle.data[base + i * items + 3].help = "Distance";
        fle.data[base + i * items + 4].help = "Angle (degrees)";
        fle.data[base + i * items + 5].help = "Emitted Intensity";
    }
}

namespace
{
    std::string make_component_dir_name(int base_id, int offset)
    {
        return component_type::cpu_names[(int)base_id] + "_" + std::to_string(offset) + "_HW";
    }
}

void update_alive_ids(cpu_state& cpu, const std::vector<size_t>& ids)
{
    for(int i=0; i < (int)cpu.files.size(); i++)
    {
        if(cpu.files[i].owner == (size_t)-1)
            continue;

        bool found = false;

        for(int j=0; j < (int)ids.size(); j++)
        {
            if(cpu.files[i].owner == ids[j])
            {
                found = true;
                break;
            }
        }

        cpu.files[i].alive = found;
    }
}

void check_audio_hardware(cpu_state& cpu, size_t my_pid)
{
    std::optional<cpu_file*> opt_ship_file = cpu.get_create_capability_file("AUDIO", my_pid, 1, true);

    if(opt_ship_file.has_value())
    {
        cpu_file& fle = *opt_ship_file.value();

        for(int i=0; i < (int)fle.len(); i += 3)
        {
            float intens = fle[i + 0].value / 100.;
            float freq = fle[i + 1].value;
            int type = fle[i + 2].value;

            cpu.audio.add(intens, freq, (waveform::type)type);
        }

        fle.data.clear();
        fle.ensure_eof();
    }
}

void check_update_components_in_hardware(ship& s, std::vector<component>& visible_components, cpu_state& cpu, playspace_manager& play, playspace* space, room* r, std::map<int, int>& type_counts, std::string dir, std::vector<size_t>& alive_ids, size_t root_ship_pid)
{
    assert(component_type::COUNT == component_type::cpu_names.size());

    alive_ids.push_back(s._pid);

    s.current_directory = dir;

    if(dir.size() > 0)
    {
        if(s.is_ship)
        {
            std::optional<cpu_file*> opt_ship_file = cpu.get_create_capability_file(dir, s._pid, 0, true);

            if(opt_ship_file.has_value())
            {
                enforce_is_ship(*opt_ship_file.value(), s._pid, root_ship_pid);
            }
        }
        else
        {
            int tc_offset = type_counts[component_type::MATERIAL]++;

            std::string extra_name = component_type::cpu_names[component_type::MATERIAL] + "_" + std::to_string(tc_offset);

            std::optional<cpu_file*> opt_ship_file = cpu.get_create_capability_file(dir + "/" + extra_name, s._pid, 0, true);

            if(opt_ship_file.has_value())
            {
                enforce_is_ship(*opt_ship_file.value(), s._pid, root_ship_pid);
            }
        }
    }

    cpu.update_regular_files(dir, s._pid, root_ship_pid);

    if(!s.is_ship)
        return;

    for(component& c : visible_components)
    {
        int my_offset = type_counts[(int)c.base_id];
        type_counts[(int)c.base_id]++;

        std::string fullname = make_component_dir_name((int)c.base_id, my_offset);

        if(dir.size() > 0)
        {
            fullname = dir + "/" + fullname;
        }

        alive_ids.push_back(c._pid);
        c.current_directory = fullname;

        if(c.has_tag(tag_info::TAG_CPU))
            continue;

        std::optional<cpu_file*> opt_file = cpu.get_create_capability_file(fullname, c._pid, 0, true);

        cpu.update_regular_files(fullname, c._pid, root_ship_pid);

        if(opt_file.has_value())
        {
            enforce_is_component(*opt_file.value(), s._pid, c._pid, root_ship_pid);

            cpu_file& file = *opt_file.value();

            if(file.len() > 0 && file[0].is_int() && file[0].value >= 0)
            {
                c.set_activation_level(file[0].value / 100.);
            }

            file[0].set_int(-1);
            file[0].help = "Set Activation Level Hardware Mapped IO";

            file[1].set_int(c.activation_level * 100);
            file[1].help = "Activation level %";

            file[2].set_int(c.get_hp_frac() * 100);
            file[2].help = "HP %";

            file[3].set_int(c.get_operating_efficiency() * 100);
            file[3].help = "Operating Efficiency %";

            file[4].set_int(c.get_fixed_props().get_heat_produced_at_full_usage(c.current_scale) * 100);
            file[4].help = "Max Heat Produced At 100% Operating Efficiency, * 100";

            float max_power_draw = 0;

            if(c.has(component_info::POWER))
            {
                max_power_draw = c.get_fixed(component_info::POWER).recharge;
            }

            file[5].set_int(max_power_draw);
            file[5].help = "Max Power Draw";

            file[6].set_int(c.get_my_temperature());
            file[6].help = "Temperature (K)";

            ///???
            if(c.has_tag(tag_info::TAG_WEAPON))
            {
                file[7].set_int(c.last_could_use);
                file[7].help = "Can be fired";

                if(file[8].is_int() && file[8].value != INT_MIN)
                {
                    c.use_angle = d2r(file[8].value);
                }

                file[8].set_int(INT_MIN);
                file[8].help = "Target Angle Hardware Mapped IO";

                file[9].set_int(round(r2d(c.use_angle)));
                file[9].help = "Current Weapon Angle";

                if(file[10].is_int() && file[10].value > 0)
                {
                    c.try_use = true;
                }

                file[10].set_int(-1);
                file[10].help = "Activate Weapon Hardware Mapped IO";

                file[11].set_int(c.last_activation_successful);
                file[11].help = "Was last activation successful?";
            }

            if(c.base_id == component_type::RADAR)
            {
                if(file[7].is_int() && file[7].value > 0)
                {
                    c.radar_offset_angle = d2r(file[7].value);
                }

                file[7].set_int(round(r2d(c.radar_offset_angle)));
                file[7].help = "Radar Direction (degrees) Hardware Mapped IO";

                if(file[8].is_int() && file[8].value > 0)
                {
                    c.radar_restrict_angle = d2r(file[8].value);
                }

                float in_deg = round(r2d(c.radar_restrict_angle));

                file[8].set_int(in_deg);
                file[8].help = "Radar Send Angle (degrees) Hardware Mapped IO";
            }

            if(c.has(component_info::SELF_DESTRUCT))
            {
                if(file[8].is_int() && file[8].value > 0)
                {
                    c.try_use = true;
                }

                ///no need to report success case
                file[8].set_int(0);
                file[8].help = "Activate Self Destruct Hardware Mapped IO";
            }
        }

        std::map<int, int> them_type_counts;
        std::unordered_map<std::string, int> ships;

        for(ship& ns : c.stored)
        {
            if(ns.is_ship)
            {
                int mcount = ships[ns.blueprint_name];

                std::map<int, int> ship_type;
                ships[ns.blueprint_name]++;

                std::string sname = fullname + "/" + ns.blueprint_name + "_" + std::to_string(mcount);
                check_update_components_in_hardware(ns, ns.components, cpu, play, space, r, ship_type, sname, alive_ids, root_ship_pid);
            }
            else
                check_update_components_in_hardware(ns, ns.components, cpu, play, space, r, them_type_counts, fullname, alive_ids, root_ship_pid);
        }
    }
}

void set_cpu_file_stored_impl(cpu_file& fle, ship& s, std::map<int, int>& type_counts, std::string dir)
{
    std::string fulldir = fle.get_fulldir_name();

    if(dir == fulldir)
    {
        fle.stored_in = s._pid;
        return;
    }

    ///cannot store a file in a material
    if(!s.is_ship)
        return;

    for(component& c : s.components)
    {
        int my_offset = type_counts[(int)c.base_id];
        type_counts[(int)c.base_id]++;

        std::string fullname = make_component_dir_name((int)c.base_id, my_offset);

        if(dir.size() > 0)
        {
            fullname = dir + "/" + fullname;
        }

        if(fullname == fulldir)
        {
            fle.stored_in = c._pid;
            return;
        }

        std::map<int, int> them_type_counts;
        std::unordered_map<std::string, int> ships;

        for(ship& ns : c.stored)
        {
            if(ns.is_ship)
            {
                int mcount = ships[ns.blueprint_name];

                std::map<int, int> ship_type;
                ships[ns.blueprint_name]++;

                std::string sname = fullname + "/" + ns.blueprint_name + "_" + std::to_string(mcount);
                set_cpu_file_stored_impl(fle, ns, ship_type, sname);
            }
            else
                set_cpu_file_stored_impl(fle, ns, them_type_counts, fullname);
        }
    }
}

void set_cpu_file_stored(ship& s, cpu_file& fle)
{
    std::map<int, int> types;

    set_cpu_file_stored_impl(fle, s, types, "");

    if(fle.stored_in == (size_t)-1)
        fle.stored_in = s._pid;
}

void update_all_ship_hardware(ship& s, cpu_state& cpu, playspace_manager& play, playspace* space, room* r, bool is_foreign = false);

void get_all_ids(std::vector<component>& in, std::set<size_t>& out)
{
    for(component& c : in)
    {
        out.insert(c._pid);

        for(ship& next : c.stored)
        {
            out.insert(next._pid);

            get_all_ids(next.components, out);
        }
    }
}

void import_foreign_ships(ship& s, cpu_state& cpu, playspace_manager& play, playspace* space, room* r, std::vector<size_t>& alive_ids)
{
    if(!r)
        return;

    std::map<int, int> type_counts;
    std::vector<std::pair<ship, std::vector<component>>> nearby = r->get_nearby_accessible_ships(s);

    for(auto& i : nearby)
    {
        ///if we dump files in, check_update_components will correct their directory
        ///the main problem is that ideally we need to continually dump files, whereas the cpu expects files to be stateful
        ///so. This is a bit of a problem
        ///what I could do is copy all files off the other ship, just wholesale import everything into our ship
        ///every file we drag in would be appended with FOREIGN/ID/ + name
        ///got ship ids, can track alive ids

        std::string extra_prefix = "FOREIGN/" + std::to_string(i.first._pid);

        for(component& c : i.first.components)
        {
            if(c.base_id != component_type::CPU)
                continue;

            update_all_ship_hardware(i.first, c.cpu_core, play, space, r, true);
        }

        std::set<size_t> visible;
        get_all_ids(i.second, visible);

        for(component& c : i.first.components)
        {
            if(c.base_id != component_type::CPU)
                continue;

            std::vector<cpu_file> their_files = c.cpu_core.files;

            for(cpu_file& i : their_files)
                alive_ids.push_back(i.owner);

            for(cpu_file& theirs : their_files)
            {
                if(theirs.root_ship_pid != i.first._pid)
                    continue;

                if(theirs.is_ship || theirs.is_component)
                {
                    if(visible.count(theirs.ship_pid) == 0)
                        continue;
                }

                ///regular file
                if(!theirs.is_ship && !theirs.is_component)
                {
                    if(visible.count(theirs.stored_in) == 0)
                        continue;
                }

                bool found = false;

                for(int idx = 0; idx < (int)cpu.files.size(); idx++)
                {
                    if(cpu.context.held_file == idx)
                        continue;

                    ///not a foreign file
                    if(cpu.files[idx].root_ship_pid != theirs.root_ship_pid)
                        continue;

                    if(cpu.files[idx].name.as_uniform_string() == extra_prefix + "/" + theirs.name.as_uniform_string())
                    {
                        cpu.files[idx] = theirs;
                        cpu.files[idx].name.set_label(extra_prefix + "/" + theirs.name.as_uniform_string());
                        found = true;
                        break;
                    }
                }

                if(!found)
                {
                    cpu_file next = theirs;
                    next.name.set_label(extra_prefix + "/" + theirs.name.as_uniform_string());

                    cpu.files.push_back(next);
                }
            }
        }
    }
}

void update_all_ship_hardware(ship& s, cpu_state& cpu, playspace_manager& play, playspace* space, room* r, bool is_foreign)
{
    std::map<int, int> type_counts;
    std::vector<size_t> ids;

    check_update_components_in_hardware(s, s.components, cpu, play, space, r, type_counts, "", ids, s._pid);

    if(!is_foreign)
    {
        dump_radar_data_into_cpu(cpu, s, play, space, r);
        check_audio_hardware(cpu, cpu._pid);
        import_foreign_ships(s, cpu, play, space, r, ids);
    }

    update_alive_ids(cpu, ids);

    ///this step is very ordering dependent
    ///do not move before update_all_ship_hardware
    ///also unsure if its necessary anymore entirely but what can you do
    for(int i=0; i < (int)cpu.files.size(); i++)
    {
        set_cpu_file_stored(s, cpu.files[i]);
    }
}

void strip_whitespace(std::string& in)
{
    while(in.size() > 0 && isspace(in[0]))
        in.erase(in.begin());

    while(in.size() > 0 && isspace(in.back()))
        in.pop_back();
}

bool all_whitespace(const std::string& in)
{
    for(int i=0; i < (int)in.size(); i++)
    {
        if(!isspace(in[i]))
            return false;
    }

    return true;
}

inline
std::vector<std::string>& split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

inline
std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

bool all_numeric(const std::string& str)
{
    try
    {
        std::stoi(str, nullptr, 0);
        return true;
    }
    catch(...)
    {
        return false;
    }
}

void register_value::make(const std::string& str)
{
    if(str.size() == 0)
        throw std::runtime_error("Bad register value, 0 length");

    if(all_numeric(str))
    {
        try
        {
            int val = std::stoi(str, nullptr, 0);

            set_int(val);

            return;
        }
        catch(...)
        {
            throw std::runtime_error("Not a valid integer " + str);
        }
    }

    if(str.size() == 1)
    {
        if(str[0] == 'G' || str[0] == 'X')
        {
            set_reg(registers::GENERAL_PURPOSE0);
            return;
        }
        else if(str[0] == 'T')
        {
            set_reg(registers::TEST);
            return;
        }
        else
        {
            for(int i=0; i < (int)registers::COUNT; i++)
            {
                if(str == registers::rnames[i])
                {
                    set_reg((registers::type)i);
                    return;
                }
            }

            if(isalpha(str[0]))
            {
                throw std::runtime_error("Bad register " + str);
            }
        }

        if(str[0] == '>' || str[0] == '<' || str[0] == '=')
        {
            set_symbol(str);
            return;
        }
    }

    if(str.size() == 2)
    {
        if(str[0] == 'X' || str[0] == 'G')
        {
            int num = str[1] - '0';

            if(num == 0)
            {
                set_reg(registers::GENERAL_PURPOSE0);
            }
            else
            {
                throw std::runtime_error("No G/X registers available > 0");
            }

            return;
        }
    }

    if(str.size() >= 2)
    {
        if(str.front() == '\"' && str.back() == '\"')
        {
            std::string stripped = str;
            stripped.erase(stripped.begin());
            stripped.pop_back();

            set_symbol(stripped);
            return;
        }

        if(str.front() == '[' && str.back() == ']')
        {
            std::string stripped = str;
            stripped.erase(stripped.begin());
            stripped.pop_back();

            for(int kk=0; kk < (int)stripped.size(); kk++)
            {
                if(isspace(stripped[kk]))
                {
                    stripped.erase(stripped.begin() + kk);
                    kk--;
                    continue;
                }
            }

            register_value fval;

            try
            {
                fval.make(stripped);
            }
            catch(std::runtime_error& err)
            {
                std::string what = err.what();

                what = "Error in address operator: " + what;

                throw std::runtime_error(what);
            }

            if(!fval.is_int() && !fval.is_reg())
                throw std::runtime_error("Element in address must be integer or register, got " + fval.as_string());

            if(fval.is_int())
                set_address(fval.value);

            if(fval.is_reg())
                set_reg_address(fval.reg);

            return;
        }

        if(str.front() == '\"' && str.back() != '\"')
            throw std::runtime_error("String not terminated");

        if(str.front() == '[' && str.back() != ']')
            throw std::runtime_error("Unterminated address, missing ]");

        set_label(str);
    }
}

register_value& instruction::fetch(int idx)
{
    if(idx < 0 || idx >= (int)args.size())
        throw std::runtime_error("Bad instruction idx " + std::to_string(idx));

    return args[idx];
}

cpu_file::cpu_file()
{
    ensure_eof();
}

int cpu_file::len()
{
    return (int)data.size() - 1;
}

int cpu_file::len_with_eof()
{
    return data.size();
}

bool cpu_file::ensure_eof()
{
    if((int)data.size() == 0 || !data.back().is_eof())
    {
        register_value fake;
        fake.set_eof();

        data.push_back(fake);

        if(file_pointer >= (int)data.size())
        {
            ///resets to EOF
            file_pointer = len();
        }

        return true;
    }

    if(file_pointer >= (int)data.size())
    {
        ///resets to EOF
        file_pointer = len();
    }

    return false;
}

void cpu_file::set_size(int next_size)
{
    if(next_size == len())
        return;

    ///pop eof
    data.pop_back();

    if(next_size > (int)data.size())
    {
        for(int kk=0; kk < (int)next_size; kk++)
        {
            register_value next;
            next.set_int(0);

            data.push_back(next);
        }
    }

    if(next_size < (int)data.size())
    {
        data.resize(next_size);
    }

    ///push eof
    ensure_eof();
}

void cpu_file::clear()
{
    set_size(0);
}

register_value& cpu_file::operator[](int idx)
{
    if(idx < 0)
        throw std::runtime_error("IDX < 0 in cpu file operator[]");

    if(idx >= len())
        set_size(idx + 1);

    return data[idx];
}

register_value& cpu_file::get_with_default(int idx, int val)
{
    if(idx < 0)
        throw std::runtime_error("IDX < 0 in cpu file operator[]");

    int last_size = len();

    if(idx >= len())
        set_size(idx + 1);

    for(int i=last_size; i < len(); i++)
    {
        assert(i < (int)data.size());

        data[i].set_int(val);
    }

    return data[idx];
}

std::string cpu_file::get_ext_name()
{
    std::string as_str = name.as_string();

    auto post_split = split(as_str, '/');

    if(post_split.size() == 0)
        return "";

    return post_split.back();
}

std::string cpu_file::get_fulldir_name()
{
    auto post_split = split(name.as_string(), '/');

    if(post_split.size() <= 1)
        return "";

    std::string ret;

    for(int i=0; i < (int)post_split.size() - 1; i++)
    {
        ret += post_split[i] + "/";
    }

    if(ret.size() > 0 && ret.back() == '/')
        ret.pop_back();

    return ret;
}

bool cpu_file::in_exact_directory(const std::string& full_dir)
{
    if(full_dir == "." || full_dir == "./" || full_dir == "")
        return split(name.as_string(), '/').size() == 0;

    return name.as_string() == (full_dir + "/" + get_ext_name());
}

bool cpu_file::in_sub_directory(const std::string& full_dir)
{
    //std::cout << "is " << name.as_uniform_string() << " in " << full_dir << " 1check " << in_exact_directory(full_dir) << " 2check " << name.as_uniform_string().starts_with(full_dir) << std::endl;

    if(in_exact_directory(full_dir))
        return true;

    return name.as_uniform_string().starts_with(full_dir);
}

std::string register_value::as_string() const
{
    if(is_reg())
    {
        return registers::rnames[(int)reg];
    }

    if(is_int())
    {
        return std::to_string(value);
    }

    if(is_symbol())
    {
        return + "\"" + symbol + "\"";
    }

    if(is_label())
    {
        return label;
    }

    if(which == 4)
    {
        return "[" + std::to_string(address) + "]";
    }

    if(which == 5)
    {
        return "[" + registers::rnames[(int)reg_address] + "]";
    }

    if(is_eof())
    {
        return "EOF";
    }

    return "ERR";

    //throw std::runtime_error("Bad register val?");
}

std::string register_value::as_uniform_string() const
{
    if(is_symbol())
        return symbol;

    return as_string();
}

register_value& register_value::decode(cpu_state& state, cpu_stash& stash)
{
    if(is_reg())
    {
        return state.fetch(stash, reg);
    }

    if(is_address())
    {
        if(stash.held_file == -1)
            throw std::runtime_error("No file held, caused by [address] " + as_string());

        cpu_file& fle = state.files[stash.held_file];

        int check_address = address;

        if(which == 5)
        {
            register_value& nval = stash.register_states[(int)reg_address];

            if(!nval.is_int())
            {
                throw std::runtime_error("Expected register in address operator to be int, got " + nval.as_string());
            }

            check_address = nval.value;
        }

        if(check_address < 0)
            throw std::runtime_error("Address < 0 " + as_string());

        if(check_address >= (int)fle.data.size())
            throw std::runtime_error("Address out of bounds " + as_string() + " in file " + fle.name.as_string());

        return fle.data[check_address];
    }

    return *this;
}

int instruction::num_args()
{
    return args.size();
}

instructions::type instructions::fetch(const std::string& name)
{
    for(int i=0; i < (int)instructions::rnames.size(); i++)
    {
        if(instructions::rnames[i] == name)
            return (instructions::type)i;
    }

    return instructions::COUNT;
}

void instruction::make(const std::vector<std::string>& raw, cpu_state& cpu)
{
    //if(raw.size() == 0)
    //    throw std::runtime_error("Bad instr");

    if(raw.size() == 0)
    {
        type = instructions::NOTE;
        return;
    }

    bool all_space = true;

    for(auto& i : raw)
    {
        if(!all_whitespace(i))
        {
            all_space = false;
        }
    }

    if(all_space)
    {
        type = instructions::NOTE;
        return;
    }

    instructions::type found = instructions::fetch(raw[0]);

    if(found == instructions::COUNT)
    {
        for(custom_instruction& cinst : cpu.custom)
        {
            if(raw[0] == cinst.name)
            {
                type = instructions::CALL;

                for(int i=0; i < (int)raw.size(); i++)
                {
                    register_value val;
                    val.make(raw[i]);

                    args.push_back(val);
                }

                return;
            }
        }

        throw std::runtime_error("Bad instruction " + raw[0]);
    }

    type = found;

    for(int i=1; i < (int)raw.size(); i++)
    {
        register_value val;
        val.make(raw[i]);

        args.push_back(val);
    }

    if(type == instructions::AT_DEF)
    {
        if(args.size() == 0)
            throw std::runtime_error("No args, use @DEF <NAME> <ARGS...>");

        if(!args[0].is_label())
            throw std::runtime_error("Invalid name, use @DEF NAME from " + args[0].as_string());

        custom_instruction cust;
        cust.name = args[0].label;

        for(int i=1; i < (int)args.size(); i++)
        {
            if(!args[i].is_label())
                throw std::runtime_error("@DEF args must be literal names, eg @DEF FUNC A0, got " + args[i].as_string());

            cust.args.push_back(args[i].label);
        }

        cpu.custom.push_back(cust);
    }
}

std::string get_next(const std::string& in, int& offset, bool& hit_comment)
{
    if(offset >= (int)in.size())
        return "";

    std::string ret;
    bool in_string = false;
    bool in_addr = false;

    bool was_addr = false;

    for(; offset < (int)in.size(); offset++)
    {
        char next = in[offset];

        if(next == '\"')
        {
            in_string = !in_string;
        }

        if(next == '[')
        {
            in_addr = true;
            was_addr = true;
        }

        if(next == ']')
        {
            in_addr = false;
        }

        if(next == ';' && !in_string && !in_addr)
        {
            hit_comment = true;
            offset++;
            return ret;
        }

        if(next == ' ' && !in_string && !in_addr)
        {
            offset++;
            return ret;
        }

        ret += next;
    }

    if(was_addr)
    {
        strip_whitespace(ret);
    }

    return ret;
}

void instruction::make(const std::string& str, cpu_state& cpu)
{
    int offset = 0;

    std::vector<std::string> vc;

    while(1)
    {
        bool hit_comment = false;

        auto it = get_next(str, offset, hit_comment);

        if(it == "")
            break;

        vc.push_back(it);

        if(hit_comment)
            break;
    }

    make(vc, cpu);
}

cpu_stash::cpu_stash()
{
    register_states.resize(registers::COUNT);

    for(int i=0; i < registers::COUNT; i++)
    {
        register_value val;
        val.set_int(0);

        register_states[i] = val;
    }
}

cpu_state::cpu_state()
{
    ports.resize(hardware::COUNT);
    blocking_status.resize(hardware::COUNT);

    for(int i=0; i < (int)hardware::COUNT; i++)
    {
        register_value val;
        val.set_int(-1);

        ports[i] = val;
    }

    update_length_register();

    update_master_virtual_file();
}

struct register_bundle
{
    cpu_stash& stash;
    register_value& in;

    register_bundle(cpu_stash& st, register_value& r) : stash(st), in(r){}

    register_value& decode(cpu_state& st)
    {
        return in.decode(st, stash);
    }
};

register_value& restricta(register_value& in, const std::string& types)
{
    std::map<char, std::string> rs
    {
    {'R', "register"},
    {'L', "label"},
    {'A', "address"},
    {'N', "integer"},
    {'S', "symbol"},
    {'Z', "EOF"},
    };

    bool should_throw = false;

    if(in.is_reg() && types.find('R') == std::string::npos)
        should_throw = true;
    if(in.is_label() && types.find('L') == std::string::npos)
        should_throw = true;
    if(in.is_address() && types.find('A') == std::string::npos)
        should_throw = true;
    if(in.is_int() && types.find('N') == std::string::npos)
        should_throw = true;
    if(in.is_symbol() && types.find('S') == std::string::npos)
        should_throw = true;
    if(in.is_eof() && types.find('Z') == std::string::npos)
        should_throw = true;

    if(should_throw)
    {
        std::string err = "Expected one of: ";

        for(auto i : types)
        {
            err += rs[i] + ", ";
        }

        err += "got " + in.as_string();

        throw std::runtime_error(err);
    }

    return in;
}

register_bundle restricta(const register_bundle& bun, const std::string& types)
{
    register_value& val = restricta(bun.in, types);

    return register_bundle(bun.stash, val);
}

///problem is we're not early decoding
///really need to pass through variable but need to bundle stash with it
///then decode right at the end
register_bundle check_environ(cpu_state& st, cpu_stash& stash, register_value& in)
{
    if(!in.is_label())
        return register_bundle(stash, in);

    if(stash.my_argument_names.size() != stash.called_with.size())
        throw std::runtime_error("Logic error in CPU arg name stuff [developer's fault]");

    for(int i=0; i < (int)stash.my_argument_names.size(); i++)
    {
        if(stash.my_argument_names[i] == in.label)
        {
            register_value& their_value = stash.called_with[i].first;
            int stk = stash.called_with[i].second;

            if(stk < 0 || stk >= (int)st.all_stash.size())
                throw std::runtime_error("STK OUT OF BOUNDS, STACK OVERFLOW " + std::to_string(stk));

            cpu_stash& their_stash = st.all_stash[stk];

            return check_environ(st, their_stash, their_value);
        }
    }

    return register_bundle(stash, in);
}

#define RA(x, y) restricta(x, #y)
#define CHECK(x) check_environ(*this, context, x)
#define DEC(x) x.decode(*this)

#define R(x) DEC(RA(CHECK(x), RA))
#define RN(x) RA(DEC(RA(CHECK(x), RAN)), N)
#define RNS(x) RA(DEC(RA(CHECK(x), RANS)), NS)
#define RNLS(x) RA(DEC(RA(CHECK(x), RANLS)), NLS)
#define RLS(x) RA(DEC(RA(CHECK(x), RALS)), LS)
#define RS(x) RA(DEC(RA(CHECK(x), RAS)), S)
#define E(x) DEC(RA(CHECK(x), RLANS))
#define L(x) DEC(RA(CHECK(x), L))

#define SYM(x) RA(x, S)
#define NUM(x) RA(x, N)

#define A1(x) x(next[0])
#define A2(x, y) x(next[0]), y(next[1])
#define A3(x, y, z) x(next[0]), y(next[1]), z(next[2])

#define CALL1(x, y) x(y(next[0]))
#define CALL2(x, y, z) x(y(next[0]), z(next[1]))
#define CALL3(x, y, z, w) x(y(next[0]), z(next[1]), w(next[2]))

#include "instructions.hpp"

bool should_skip(cpu_state& s)
{
    if(s.inst.size() == 0)
        return false;

    int npc = s.context.pc % (int)s.inst.size();

    if(npc < 0 || npc >= (int)s.inst.size())
        throw std::runtime_error("Bad pc somehow");

    instruction& next = s.inst[npc];

    if(next.type == instructions::COUNT)
        throw std::runtime_error("Bad instruction at runtime?");

    return next.type == instructions::NOTE || next.type == instructions::MARK;
}

cpu_file cpu_state::get_master_virtual_file()
{
    cpu_file fle;

    fle.data.clear();

    fle.data.reserve(files.size());

    for(int i=0; i < (int)files.size(); i++)
    {
        fle.data.push_back(files[i].name);
    }

    fle.name.set_label("FILES");

    fle.ensure_eof();

    return fle;
}

/*cpu_file cpu_state::get_master_foreign_file()
{
    cpu_file fle;

    fle.data.clear();

    fle.data.reserve(files.size());

    for(int i=0; i < (int)files.size(); i++)
    {
        fle.data.push_back(files[i].name);
    }

    fle.name.set_label("FILES");

    fle.ensure_eof();

    return fle;
}*/

bool cpu_state::update_master_virtual_file()
{
    register_value val;
    val.set_label("FILES");

    auto id_opt = name_to_file_id(val);

    if(id_opt)
    {
        if(any_holds(id_opt.value()))
        {
            return false;
        }
        else
        {
            files[id_opt.value()] = get_master_virtual_file();
            return false;
        }
    }

    ///file not present
    files.push_back(get_master_virtual_file());
    update_master_virtual_file();

    return true;
}

bool cpu_state::any_blocked()
{
    for(int i=0; i < (int)blocking_status.size(); i++)
    {
        if(blocking_status[i])
            return true;
    }

    return false;
}

struct eof_helper
{
    cpu_state& st;

    eof_helper(cpu_state& in) : st(in) {}

    ~eof_helper()
    {
        if(st.context.held_file != -1)
        {
            if(st.files[st.context.held_file].ensure_eof())
            {
                st.update_length_register();
            }
        }
    }
};

struct f_register_helper
{
    cpu_state& st;

    f_register_helper(cpu_state& in) : st(in) {}

    ~f_register_helper()
    {
        st.update_f_register();
    }
};

void cpu_state::step(std::shared_ptr<ship> s, playspace_manager* play, playspace* space, room* r)
{
    try
    {
        ustep(s, play, space, r);
    }
    catch(std::runtime_error& err)
    {
        last_error = err.what();
        free_running = false;
    }
}

bool entity_visible_to(const std::shared_ptr<entity>& e, ship& s)
{
    for(auto& i : s.last_sample.renderables)
    {
        if(e->_pid == i.uid)
            return true;
    }

    return false;
}

void cpu_state::ustep(std::shared_ptr<ship> s, playspace_manager* play, playspace* space, room* r)
{
    if(waiting_for_hardware_feedback || tx_pending)
        return;

    should_step = false;

    ///shit we might double tx which makes dirty filing not work
    ///need to rename correctly
    /*if(had_tx_pending)
    {
        for(int i=0; i < (int)files.size(); i++)
        {
            if(files[i].was_xferred)
            {
                remove_file(i);
                i--;
                continue;
            }
        }

        had_tx_pending = false;
    }*/

    if(had_tx_pending && s)
    {
        update_all_ship_hardware(*s, *this, *play, space, r);

        had_tx_pending = false;
    }

    check_for_bad_files();

    if(inst.size() == 0)
        return;

    context.pc %= (int)inst.size();

    if(context.pc < 0 || context.pc >= (int)inst.size())
        throw std::runtime_error("Bad pc somehow");

    using namespace instructions;

    instruction& next = inst[context.pc];

    if(next.type == instructions::COUNT)
        throw std::runtime_error("Bad instruction at runtime?");

    ///so whenever we get a label
    ///need to run it through our list

    //std::cout << "NEXT " << instructions::rnames[(int)next.type] << std::endl;

    ///ensure that whatever happens, our file has an eof at the end
    eof_helper eof_help(*this);
    f_register_helper f_help(*this);

    switch(next.type)
    {
    case COPY:
        CALL2(icopy, E, R);
        break;
    case ADDI:
        CALL3(iaddi, RNLS, RNLS, R);
        break;
    case SUBI:
        CALL3(isubi, RN, RN, R);
        break;
    case MULI:
        CALL3(imuli, RN, RN, R);
        break;
    case DIVI:
        CALL3(idivi, RN, RN, R);
        break;
    case MODI:
        CALL3(imodi, RN, RN, R);
        break;
    case MARK:///pseudo instruction
        //pc++;
        //step();
        //return;
        break;
    case SWIZ:
        CALL3(iswiz, RNLS, RN, R);
        break;
    case JUMP:
        context.pc = label_to_pc(L(next[0]).label) + 1;
        return;
        break;
    case TJMP:
        if(fetch(context, registers::TEST).is_int() && fetch(context, registers::TEST).value != 0)
        {
            context.pc = label_to_pc(L(next[0]).label) + 1;
            return;
        }

        break;
    case FJMP:
        if(fetch(context, registers::TEST).is_int() && fetch(context, registers::TEST).value == 0)
        {
            context.pc = label_to_pc(L(next[0]).label) + 1;
            return;
        }

        break;

    case TEST:
        if(next.num_args() == 1 && next[0].is_label())
        {
            if(context.held_file == -1)
                throw std::runtime_error("Not holding file [TEST EOF]");

            if(next[0].label == "EOF")
            {
                cpu_file& held = files[context.held_file];

                int fptr = held.file_pointer;

                fetch(context, registers::TEST).set_int(held.data[fptr].is_eof());
                break;
            }
            else
            {
                throw std::runtime_error("TEST expects TEST VAL OP VAL or TEST EOF");
            }
        }

        itest(RNLS(next[0]), SYM(next[1]), RNLS(next[2]), fetch(context, registers::TEST));
        break;
    case SLEN:
    {
        register_value& val = RLS(next[0]);
        register_value& dest = R(next[1]);

        if(val.is_label())
        {
            dest.set_int(val.label.size());
        }
        else if(val.is_symbol())
        {
            dest.set_int(val.symbol.size());
        }
        else
        {
            throw std::runtime_error("Not label or symbol in [SLEN]");
        }

        break;
    }
    case HALT:
        throw std::runtime_error("Received HALT");
        break;
    case WAIT:
            if(any_blocked())
                return;
            break;
    case HOST:
        ///name of ship

        if(s == nullptr)
            throw std::runtime_error("Host instruction can only be used if we are a ship");

        R(next[0]).set_int(s->_pid);

        break;
    case MAKE:
        if(context.held_file != -1)
            throw std::runtime_error("Already holding file [MAKE] " + files[context.held_file].name.as_string());

        if(next.num_args() == 0)
        {
            cpu_file fle;
            fle.name.set_int(get_next_persistent_id());

            if(s)
                fle.root_ship_pid = s->_pid;

            files.push_back(fle);

            context.held_file = (int)files.size() - 1;
            update_length_register();
        }
        else if(next.num_args() == 1)
        {
            register_value& name = RNLS(next[0]);

            for(auto& i : files)
            {
                if(i.name == name)
                    throw std::runtime_error("Duplicate file name [MAKE] " + name.as_string());
            }

            cpu_file fle;
            fle.name = name;

            if(s)
                fle.root_ship_pid = s->_pid;

            files.push_back(fle);

            context.held_file = (int)files.size() - 1;
            update_length_register();
        }
        else
        {
            throw std::runtime_error("MAKE takes 0 or 1 args");
        }

        if(s)
        {
            set_cpu_file_stored(*s, files[context.held_file]);

            if(files[context.held_file].stored_in == (size_t)-1)
                throw std::runtime_error("BAD MAKE, ID -1 SHOULD BE IMPOSSIBLE");
        }

        break;
    case RNAM:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [RNAM]");

        {

            register_value& name = E(next[0]);

            for(auto& i : files)
            {
                if(i.name == name)
                    throw std::runtime_error("Duplicate file name [RNAM] " + name.as_string());
            }

            files[context.held_file].name = name;
            files[context.held_file].owner = -1;

            ///incase we rename the master virtual file
            update_master_virtual_file();

            if(s)
            {
                set_cpu_file_stored(*s, files[context.held_file]);

                if(files[context.held_file].stored_in == (size_t)-1)
                    throw std::runtime_error("BAD RNAM, ID -1 SHOULD BE IMPOSSIBLE");
            }
        }

        break;

    case RSIZ:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [RSIZ]");

        {
            cpu_file& cur = files[context.held_file];
            int next_size = NUM(RN(next[0])).value;

            if(next_size > 1024 * 1024)
                throw std::runtime_error("Files are limited to 1KiB of storage");

            if(next_size < 0)
                throw std::runtime_error("[RSIZ] argument must be >= 0");

            cur.set_size(next_size);

            update_length_register();
        }

        break;

    ///or maybe drop should just take a destination arg
    case TXFR:
        {
            if(context.held_file == -1)
                throw std::runtime_error("Not holding file [TXFR]");

            cpu_file& file = files[context.held_file];

            if(!file.is_ship)
                throw std::runtime_error("Only a ship or materials can be [TXFR]'d");

            std::string to_name = E(next[0]).as_string();

            std::optional<cpu_file*> destination_file = get_file_by_name(to_name);

            if(!destination_file.has_value())
                throw std::runtime_error("No destination component with the name " + to_name + " [TXFR]");

            if(!destination_file.value()->is_component)
                throw std::runtime_error("Destination named " + to_name + " must be component in [TXFR]");

            size_t pid_of_ship_to_be_moved = file.ship_pid;

            cpu_xfer xf;
            /*xf.from = file.name.as_string();
            xf.to = E(next[0]).as_string();*/

            xf.pid_ship_from = pid_of_ship_to_be_moved;
            xf.pid_ship_to = destination_file.value()->ship_pid;
            xf.pid_component = destination_file.value()->component_pid;
            xf.is_fractiony = false;
            xf.fraction = 1;

            xf.held_file = context.held_file;
            xfers.push_back(xf);
            ///so
            ///when xferring stuff around we can guarantee no duplicates because the names are uniquely generated
            ///unless someone's put something there already, in which case it should get trampled? ignore that for the moment
            ///can make it illegal to name things in a certain way?
            ///ok so
            ///need to pretend that the file we're holding has gone somewhere else
            ///need to delete any files with that name already, or deliberately overwrite
            ///involes fixing all held files up, not too much of a problem
            ///although... if someone's holding it that's a problem
            ///so maybe naming scheme is way to go

            ///stalls
            tx_pending = true;
            had_tx_pending = true;
        }

        break;

    case GRAB:
    {
        if(context.held_file != -1)
            throw std::runtime_error("Already holding file " + files[context.held_file].name.as_string());

        register_value& to_grab = RNLS(next[0]);

        if(s)
        {
            update_all_ship_hardware(*s, *this, *play, space, r);
        }

        if(to_grab.is_label() && to_grab.label == "FILES")
        {
            update_master_virtual_file();
        }

        auto grab_opt = get_grabbable_file(to_grab);

        if(!grab_opt)
            throw std::runtime_error("No such file, or already held by someone else [GRAB]");

        context.held_file = grab_opt.value();

        update_length_register();

        break;
    }
    case FASK:
    {
        bool success = false;

        if(context.held_file != -1)
        {
            success = false;
        }
        else
        {
            register_value& to_grab = RNLS(next[0]);

            if(to_grab.is_label() && to_grab.label == "FILES")
            {
                update_master_virtual_file();
            }

            auto grab_opt = get_grabbable_file(to_grab);

            if(grab_opt)
            {
                success = true;
                context.held_file = grab_opt.value();
                update_length_register();
            }
        }

        context.register_states[(int)registers::TEST].set_int(success);

        break;
    }

    case ISHW:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [ISHW]");

        context.register_states[(int)registers::TEST].set_int(files[context.held_file].owner != (size_t)-1);

        break;

    case instructions::FILE:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [FILE]");

        R(next[0]) = files[context.held_file].name;
        break;

    case FIDX:
    {
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [FIDX]");

        R(next[0]).set_int(files[context.held_file].file_pointer);
        break;
    }


    case SEEK:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [SEEK]");

        {
            int off = RN(next[0]).value;

            files[context.held_file].file_pointer += off;
            files[context.held_file].file_pointer = clamp(files[context.held_file].file_pointer, 0, files[context.held_file].len());
        }

        break;


    case VOID_FUCK_WINAPI:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [VOID]");

        if(files[context.held_file].file_pointer < (int)files[context.held_file].len())
        {
            files[context.held_file].data.erase(files[context.held_file].data.begin() + files[context.held_file].file_pointer);
        }

        break;

    case DROP:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [DROP]");

        if(s)
        {
            potentially_move_file_to_foreign_ship(*play, space, r, s);
        }

        drop_file();
        update_length_register();

        if(s)
        {
            update_all_ship_hardware(*s, *this, *play, space, r);
        }

        update_master_virtual_file(); ///in case we drop the master file, now's a good time to update it
        break;

    case WIPE:
    {
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [WIPE]");

        int held = context.held_file;

        drop_file();
        remove_file(held);
        update_length_register();
        update_master_virtual_file();

        ///no need to update ship hardware, WIPE can't do anything

        break;
    }

    case NOOP:
        break;
    case NOTE:
        //pc++;
        //step();
        //return;
        break;
    case RAND:
        CALL3(irandi, RN, RN, R);
        break;
    case TIME:
        R(next[0]).set_int(get_time_ms());
        break;
    case AT_REP:
        throw std::runtime_error("Should never be executed [rep]");
    case AT_END:
        if(all_stash.size() == 0)
            throw std::runtime_error("Not in a function! @END");
        {
            context = all_stash.back();
            all_stash.pop_back();
            update_length_register();
        }
        break;
    case AT_N_M:
        throw std::runtime_error("Should never be executed [n_m]");
    case AT_DEF:
        //throw std::runtime_error("Should never be execited [@def]");
        {
            int defs = 1;

            context.pc++;

            for(; context.pc < (int)inst.size(); context.pc++)
            {
                if(inst[context.pc].type == instructions::AT_DEF)
                    defs++;

                if(inst[context.pc].type == instructions::AT_END)
                {
                    defs--;

                    if(defs == 0)
                        break;
                }
            }

            if(defs != 0)
                throw std::runtime_error("Unterminated @DEF, needs @END");
        }

        break;
    case CALL:
        {
            std::string name = L(next[0]).label;

            bool found = false;

            std::vector<std::string> args;

            for(custom_instruction& cst : custom)
            {
                if(cst.name == name)
                {
                    int my_args = next.num_args();

                    if(my_args - 1 != (int)cst.args.size())
                    {
                        throw std::runtime_error("Instruction " + name + " expects " + std::to_string(cst.args.size()) + " args, got " + std::to_string(my_args - 1));
                    }
                    else
                    {
                        args = cst.args;

                        found = true;
                    }
                }
            }

            if(!found)
                throw std::runtime_error("No such instruction to call, [" + name + "]");

            all_stash.push_back(context);

            cpu_stash next_stash;

            for(int i=1; i < next.num_args(); i++)
            {
                next_stash.called_with.push_back({next[i], (int)all_stash.size() - 1});
            }

            next_stash.my_argument_names = args;

            int npc = get_custom_instr_pc(name) + 1;

            context = next_stash;
            context.pc = npc;

            update_length_register();
        }

        return;
        break;
    case DATA:
        throw std::runtime_error("Unimpl");
    case WARP:
        ports[(int)hardware::W_DRIVE] = RNLS(next[0]);
        blocking_status[(int)hardware::W_DRIVE] = 1;
        waiting_for_hardware_feedback = true;
        break;
    case SLIP:
        if(next.num_args() == 1)
        {
            ports[(int)hardware::S_DRIVE] = RNS(next[0]);
        }
        else if(next.num_args() == 2)
        {
            ports[(int)hardware::S_DRIVE].set_symbol(RLS(next[0]).as_string() + RNS(next[1]).as_string());
        }
        else
        {
            throw std::runtime_error("SLIP expects 1 or 2 args");
        }

        blocking_status[(int)hardware::S_DRIVE] = 1;
        waiting_for_hardware_feedback = true;
        break;
    case TMOV:
    {
        register_value& val = RNLS(next[0]);

        ports[(int)hardware::T_DRIVE].set_int(1);

        my_move = decltype(my_move)();

        if(val.is_int())
            my_move.id = val.value;

        if(val.is_symbol())
            my_move.name = val.symbol;

        if(val.is_label())
            my_move.name = val.label;

        my_move.type = TMOV;

        blocking_status[(int)hardware::T_DRIVE] = 1;
        waiting_for_hardware_feedback = true;
        break;
    }
    case AMOV:
    {
        register_value& xval = RN(next[0]);
        register_value& yval = RN(next[1]);

        my_move = decltype(my_move)();

        my_move.type = AMOV;
        my_move.x = xval.value;
        my_move.y = yval.value;

        blocking_status[(int)hardware::T_DRIVE] = 1;
        waiting_for_hardware_feedback = true;

        break;
    }
    case RMOV:
    {
        register_value& xval = RN(next[0]);
        register_value& yval = RN(next[1]);

        ports[(int)hardware::T_DRIVE].set_int(1);

        my_move = decltype(my_move)();

        if(next.num_args() > 2)
        {
            register_value& val = RNLS(next[2]);

            if(val.is_int())
            {
                ///ease of use
                if(val.value == 0)
                    val.value = -1;

                my_move.id = val.value;
            }

            if(val.is_symbol())
                my_move.name = val.symbol;

            if(val.is_label())
                my_move.name = val.label;
        }

        my_move.type = RMOV;
        my_move.x = xval.value;
        my_move.y = yval.value;

        blocking_status[(int)hardware::T_DRIVE] = 1;
        waiting_for_hardware_feedback = true;
        break;
    }
    case KEEP:
    {
        int dist = RN(next[0]).value;
        int id = RN(next[1]).value;

        ports[(int)hardware::T_DRIVE].set_int(1);

        my_move = decltype(my_move)();

        my_move.type = KEEP;
        my_move.radius = dist;
        my_move.id = id;

        blocking_status[(int)hardware::T_DRIVE] = 1;
        waiting_for_hardware_feedback = true;
        break;
    }

    case TTRN:
    {
        register_value& val = RNLS(next[0]);

        ports[(int)hardware::T_DRIVE].set_int(1);

        my_move = decltype(my_move)();

        if(val.is_int())
            my_move.id = val.value;

        if(val.is_symbol())
            my_move.name = val.symbol;

        if(val.is_label())
            my_move.name = val.label;

        my_move.type = TTRN;

        blocking_status[(int)hardware::T_DRIVE] = 1;
        waiting_for_hardware_feedback = true;

        break;
    }
    case ATRN:
    {
        register_value& angle = RN(next[0]);

        ///maybe make the angle up even though i know itll make me really angry ok
        ///but you know most people just don't work in atan2 space
        ///which is fine even though they're obviously bad people
        float rang = (angle.value / 360.) * 2 * M_PI;

        ports[(int)hardware::T_DRIVE].set_int(1);
        my_move = decltype(my_move)();

        my_move.type = ATRN;
        my_move.angle = rang;

        blocking_status[(int)hardware::T_DRIVE] = 1;
        waiting_for_hardware_feedback = true;
        break;
    }
    case RTRN:
    {
        register_value& angle = RN(next[0]);
        float rang = (angle.value / 360.) * 2 * M_PI;

        ports[(int)hardware::T_DRIVE].set_int(1);
        my_move = decltype(my_move)();

        my_move.type = RTRN;
        my_move.angle = rang;

        blocking_status[(int)hardware::T_DRIVE] = 1;
        waiting_for_hardware_feedback = true;

        break;
    }
    case TFIN:
    {
        register_value& val = RN(next[0]);

        if(val.is_int())
        {
            int ival = val.value;

            if(ival == hardware::S_DRIVE)
                context.register_states[(int)registers::TEST].set_int(blocking_status[ival]);
            else if(ival == hardware::T_DRIVE)
                context.register_states[(int)registers::TEST].set_int(blocking_status[ival]);
            else if(ival == hardware::W_DRIVE)
                context.register_states[(int)registers::TEST].set_int(blocking_status[ival]);
            else
                throw std::runtime_error("No such value [TFIN] " + std::to_string(ival));
        }
        else
        {
            throw std::runtime_error("Unimplemented [TFIN]");
        }

        break;
    }

    case TANG:
    {
        if(s == nullptr || r == nullptr)
            throw std::runtime_error("Cannot be used while not in a POI, or invalid ship [TANG]");

        size_t id = RN(next[0]).value;

        std::optional<std::shared_ptr<entity>> e_opt = r->entity_manage->fetch(id);

        if(!e_opt.has_value() || !entity_visible_to(e_opt.value(), *s))
            break;

        vec2f to_them = e_opt.value()->r.position - s->r.position;

        R(next[1]).set_int(round(r2d(to_them.angle())));
        break;
    }

    case TDST:
    {
        if(s == nullptr || r == nullptr)
            throw std::runtime_error("Cannot be used while not in a POI, or invalid ship [TDST]");

        size_t id = RN(next[0]).value;

        std::optional<std::shared_ptr<entity>> e_opt = r->entity_manage->fetch(id);

        if(!e_opt.has_value() || !entity_visible_to(e_opt.value(), *s))
            break;

        vec2f to_them = e_opt.value()->r.position - s->r.position;

        R(next[1]).set_int(round(to_them.length()));
        break;
    }

    case TPOS:
    {
        if(s == nullptr || r == nullptr)
            throw std::runtime_error("Cannot be used while not in a POI, or invalid ship [TPOS]");

        size_t id = RN(next[0]).value;

        std::optional<std::shared_ptr<entity>> e_opt = r->entity_manage->fetch(id);

        if(!e_opt.has_value() || !entity_visible_to(e_opt.value(), *s))
            break;

        R(next[1]).set_int(round(e_opt.value()->r.position.x()));
        R(next[2]).set_int(round(e_opt.value()->r.position.y()));
        break;
    }

    case TREL:
    {
        if(s == nullptr || r == nullptr)
            throw std::runtime_error("Cannot be used while not in a POI, or invalid ship [TPOS]");

        size_t id = RN(next[0]).value;

        std::optional<std::shared_ptr<entity>> e_opt = r->entity_manage->fetch(id);

        if(!e_opt.has_value() || !entity_visible_to(e_opt.value(), *s))
            break;

        vec2f to_them = e_opt.value()->r.position - s->r.position;

        R(next[1]).set_int(round(to_them.x()));
        R(next[2]).set_int(round(to_them.y()));
        break;
    }

    case PAUS:
    {
        free_running = false;
        break;
    }

    case DOCK:
    {
        if(!play || !s)
            throw std::runtime_error("No playspace manager or no ship");

        auto [p, r] = play->get_location_for(s);

        if(p && r)
        {
            r->try_dock_to(s->_pid, RN(next[0]).value);
        }

        break;
    }

    case LEAV:
    {
        throw std::runtime_error("Currently impossible for this to ever be useful as a cpu cannot run while docked");

        break;
    }

    case COUNT:
        throw std::runtime_error("Unreachable?");
    }

    context.pc++;

    int num_skip = 0;

    while(should_skip(*this) && num_skip < (int)inst.size())
    {
        context.pc++;

        num_skip++;
    }
}

void cpu_state::inc_pc()
{
    free_running = false;

    should_step = true;
}

void cpu_state::reset()
{
    auto program_backup = saved_program;

    size_t old_pid = _pid;

    *this = cpu_state();

    this->_pid = old_pid;

    saved_program = program_backup;

    set_program(saved_program);
}

void cpu_state::run()
{
    free_running = true;
}

void cpu_state::stop()
{
    free_running = false;
}

void cpu_state::potentially_move_file_to_foreign_ship(playspace_manager& play, playspace* space, room* r, std::shared_ptr<ship> me)
{
    if(context.held_file == -1)
        throw std::runtime_error("BAD CHECK FILE SHOULD NOT HAPPEN IN HERE BUT ITS OK");

    if(me == nullptr || r == nullptr)
        return;

    cpu_file& fle = files[context.held_file];

    if(fle.is_ship || fle.is_component)
        return;

    std::string str = fle.name.as_uniform_string();

    if(!str.starts_with("FOREIGN/"))
        return;

    int search_str = std::string("FOREIGN/").size();

    str.erase(0, search_str);

    ///strip ship name
    while(str.size() > 0 && str.front() != '/')
    {
        str.erase(str.begin());
    }

    /// remove /
    if(str.size() > 0)
        str.erase(str.begin());

    if(str.size() == 0)
        return;

    for(cpu_file& check : files)
    {
        if(!check.is_ship && !check.is_component)
            continue;

        if(!fle.in_exact_directory(check.name.as_uniform_string()))
            continue;

        //std::vector<ship*> ships = r->entity_manage->fetch<ship>();

        auto nearby = r->get_nearby_accessible_ships(*me);

        //for(ship* s : ships)

        for(auto& near_info : nearby)
        {
            ship& s = near_info.first;

            if(s._pid != check.root_ship_pid)
                continue;

            std::optional<std::shared_ptr<entity>> freal = r->entity_manage->fetch(s._pid);

            if(!freal.has_value())
                throw std::runtime_error("Something really out of whack in cpu state potentially move file");

            ship* real_s = dynamic_cast<ship*>(freal.value().get());

            if(real_s == nullptr)
                throw std::runtime_error("Something super duper out of whack 2 in cpu state potentially move file");

            for(component& c : real_s->components)
            {
                if(c.base_id != component_type::CPU)
                    continue;

                cpu_file mcopy = fle;
                mcopy.name.set_label(str);

                c.cpu_core.files.push_back(mcopy);
                c.cpu_core.had_tx_pending = true;

                fle.alive = false;

                update_all_ship_hardware(*real_s, c.cpu_core, play, space, r, false);

                break;
            }

            break;
        }

        return;
    }
}

void cpu_state::drop_file()
{
    if(context.held_file == -1)
        throw std::runtime_error("BAD DROP FILE SHOULD NOT HAPPEN IN HERE BUT ITS OK");

    files[context.held_file].file_pointer = 0;
    context.held_file = -1;
}

void cpu_state::update_length_register()
{
    int len = -1;

    if(context.held_file != -1)
    {
        len = files[context.held_file].len();
    }

    context.register_states[(int)registers::FILE_LENGTH].set_int(len);
}

void cpu_state::update_f_register()
{
    if(context.held_file == -1)
    {
        context.register_states[(int)registers::FILE].set_int(0);
    }
    else
    {
        if(files[context.held_file].file_pointer >= (int)files[context.held_file].data.size())
            context.register_states[(int)registers::FILE].set_eof();
        else
            context.register_states[(int)registers::FILE] = files[context.held_file].data[files[context.held_file].file_pointer];
    }

}

void cpu_state::debug_state()
{
    printf("PC %i\n", context.pc);

    //for(auto& i : register_states)
    for(int i=0; i < (int)context.register_states.size(); i++)
    {
        std::string name = registers::rnames[i];
        std::string val = context.register_states[i].as_string();

        printf("%s: %s\n", name.c_str(), val.c_str());
    }

    for(int i=0; i < (int)hardware::COUNT; i++)
    {
        std::cout << "Port: " + std::to_string(i) + ": " + ports[i].as_string() << std::endl;
    }
}

register_value& cpu_state::fetch(cpu_stash& stash, registers::type type)
{
    if((int)type < 0 || (int)type >= (int)stash.register_states.size())
        throw std::runtime_error("No such register " + std::to_string((int)type));

    if(type == registers::FILE)
    {
        if(stash.held_file == -1)
            throw std::runtime_error("No file held");

        if(files[stash.held_file].file_pointer >= (int)files[stash.held_file].data.size())
            throw std::runtime_error("Invalid file pointer " + std::to_string(files[stash.held_file].file_pointer));

        int& fp = files[stash.held_file].file_pointer;

        register_value& next = files[stash.held_file].data[fp];

        fp++;

        return next;
    }

    return stash.register_states[(int)type];
}

void cpu_state::add(const std::vector<std::string>& raw)
{
    instruction i1;
    i1.make(raw, *this);

    inst.push_back(i1);
}

void cpu_state::add_line(const std::string& str)
{
    instruction i1;
    i1.make(str, *this);

    inst.push_back(i1);
}

int cpu_state::label_to_pc(const std::string& label)
{
    for(int i=0; i < (int)inst.size(); i++)
    {
        const instruction& instr = inst[i];

        if(instr.type != instructions::MARK)
            continue;

        if(instr.args.size() == 0)
            continue;

        if(!instr.args[0].is_label())
            continue;

        if(instr.args[0].label == label)
            return i;
    }

    throw std::runtime_error("Attempted to jump to non existent label: " + label);
}

int cpu_state::get_custom_instr_pc(const std::string& label)
{
    for(int i=0; i < (int)inst.size(); i++)
    {
        const instruction& instr = inst[i];

        if(instr.type != instructions::AT_DEF)
            continue;

        if(instr.args.size() == 0)
            continue;

        if(!instr.args[0].is_label())
            continue;

        if(instr.args[0].label == label)
            return i;
    }

    throw std::runtime_error("Attempted to jump to non existent custom instr: " + label);
}

void cpu_state::set_program(std::string str)
{
    try
    {
        size_t old_pid = _pid;

        *this = cpu_state();

        this->_pid = old_pid;

        if(str.size() > 10000)
            return;

        saved_program = str;

        inst.clear();

        auto all = split(str, '\n');

        for(const auto& i : all)
        {
            add_line(i);
        }
    }
    catch(std::runtime_error& err)
    {
        last_error = err.what();
        free_running = false;
    }
}

std::optional<cpu_file*> cpu_state::get_file_by_name(const std::string& fullname)
{
    for(auto& i : files)
    {
        if(i.name.is_label() && i.name.label == fullname)
            return &i;

        if(i.name.is_symbol() && i.name.symbol == fullname)
            return &i;
    }

    return std::nullopt;
}

std::optional<cpu_file*> cpu_state::get_create_capability_file(const std::string& filename, size_t owner, size_t owner_offset, bool is_hw)
{
    for(int i=0; i < (int)files.size(); i++)
    {
        //if((files[i].name.is_symbol() && files[i].name.symbol == filename) || (files[i].name.is_label() && files[i].name.label == filename))
        if(files[i].owner == owner && files[i].owner_offset == owner_offset)
        {
            files[i].is_hw = is_hw;
            files[i].alive = true;

            if(!files[i].name.is_label() || files[i].name.label != filename)
            {
                files[i].name.set_label(filename);
                //update_master_virtual_file();
            }

            if(context.held_file == i)
                return std::nullopt;

            return &files[i];
        }
    }

    cpu_file fle;
    fle.name.set_label(filename);
    fle.owner = owner;
    fle.owner_offset = owner_offset;
    fle.is_hw = is_hw;

    ///cannot invalidate held file integer
    files.push_back(fle);

    return &files.back();
}

void cpu_state::update_regular_files(const std::string& directoryname, size_t owner, size_t root_pid)
{
    for(int i=0; i < (int)files.size(); i++)
    {
        if(files[i].owner == (size_t)-1 && files[i].stored_in == owner)
        {
            std::string ext = files[i].get_ext_name();

            if(directoryname != "")
                files[i].name.set_label(directoryname + "/" + ext);
            else
                files[i].name.set_label(ext);

            files[i].root_ship_pid = root_pid;
        }
    }
}

void cpu_state::remove_file(int held_id)
{
    if(held_id == -1)
        throw std::runtime_error("Held id -1 in remove_file");

    if(held_id >= (int)files.size())
        throw std::runtime_error("Should be impossible, held_id >= files.size()");

    if(context.held_file == held_id)
        throw std::runtime_error("Bad logic error, attempted to remove file in use");

    for(cpu_stash& sth : all_stash)
    {
        if(sth.held_file == held_id)
            throw std::runtime_error("Bad logic error, attempted to remove file in use in stack");
    }

    if(context.held_file > held_id)
    {
        context.held_file--;
    }

    for(cpu_stash& sth : all_stash)
    {
        if(sth.held_file > held_id)
            sth.held_file--;
    }

    files.erase(files.begin() + held_id);
}

bool cpu_state::any_holds(int held_id)
{
    if(context.held_file == held_id)
        return true;

    for(cpu_stash& sth : all_stash)
    {
        if(sth.held_file == held_id)
            return true;
    }

    return false;
}

bool file_equiv_name(register_value& v1, register_value& v2)
{
    return v1 == v2;
}

std::optional<int> cpu_state::name_to_file_id(register_value& name)
{
    for(int i=0; i < (int)files.size(); i++)
    {
        if(files[i].name == name)
            return i;
    }

    return std::nullopt;
}

std::optional<int> cpu_state::get_grabbable_file(register_value& name)
{
    ///look through capability files first
    for(int i=0; i < (int)files.size(); i++)
    {
        cpu_file& fle = files[i];

        if(fle.owner == (size_t)-1)
            continue;

        if(any_holds(i))
            continue;

        if(fle.name == name)
            return i;
    }

    ///look through non capability files
    for(int i=0; i < (int)files.size(); i++)
    {
        cpu_file& fle = files[i];

        if(fle.owner != (size_t)-1)
            continue;

        if(any_holds(i))
            continue;

        if(fle.name == name)
            return i;
    }

    return std::nullopt;
}

std::optional<std::string> cpu_state::pid_to_file_directory(size_t stored_in)
{
    for(auto& i : files)
    {
        if(i.stored_in == stored_in)
            return i.name.as_uniform_string();
    }

    return std::nullopt;
}

void cpu_state::check_for_bad_files()
{
    for(int i=0; i < (int)files.size(); i++)
    {
        if(!files[i].alive && !any_holds(i))
        {
            remove_file(i);
            i--;
            continue;
        }
    }

    for(int i=0; i < (int)files.size(); i++)
    {
        if(files[i].owner == (size_t)-1)
            continue;

        for(int j=0; j < (int)files.size(); j++)
        {
            if(i == j)
                continue;

            if(files[j].owner != (size_t)-1)
                continue;

            if(file_equiv_name(files[i].name, files[j].name))
            {
                if(any_holds(j))
                    continue;

                printf("Cleaned\n");

                if(i > j)
                {
                    i--;
                }

                remove_file(j);
                j--;
                continue;
            }
        }
    }
}

std::vector<cpu_file> cpu_state::extract_files_in_directory(const std::string& directory)
{
    std::vector<cpu_file> ret;

    for(int i=0; i < (int)files.size(); i++)
    {
        if(files[i].in_sub_directory(directory))
        {
            if(files[i].name.as_uniform_string() == directory)
                continue;

            ret.push_back(files[i]);

            files[i].alive = false;
        }
    }

    for(cpu_file& fle : ret)
    {
        std::string lab = fle.name.as_uniform_string();

        if(!lab.starts_with(directory))
        {
            throw std::runtime_error("Unsure how this happened but it did, label " + lab + " did not start with " + directory);
        }

        assert(lab.size() >= directory.size());

        for(int i=0; i < (int)directory.size(); i++)
        {
            lab.erase(lab.begin());
        }

        if(lab.size() > 0 && lab.front() == '/')
            lab.erase(lab.begin());

        fle.name.set_label(lab);
    }

    return ret;
}

void cpu_state::nullstep()
{
    step(nullptr, nullptr, nullptr, nullptr);
}

void cpu_tests()
{
    assert(instructions::rnames.size() == instructions::COUNT);

    {
        cpu_state test;

        test.add_line("COPY 5 X");
        test.add_line("COPY 7 T");
        test.add_line("ADDI X T X");

        test.nullstep();
        test.nullstep();
        test.nullstep();

        test.debug_state();

        test.add_line("COPY 0 X");
        test.add_line("COPY 0 T");

        test.nullstep();
        test.nullstep();

        test.add_line("MARK MY_LAB");
        test.add_line("ADDI X 1 X");
        test.add_line("JUMP MY_LAB");

        test.nullstep();
        test.nullstep();
        test.nullstep();
        test.nullstep();

        test.debug_state();
    }

    {
        cpu_state test;

        test.add_line("COPY 53 X");
        test.add_line("TEST 54 > X");

        test.nullstep();
        test.nullstep();
        test.debug_state();
    }

    {
        cpu_state test;

        test.add_line("COPY 53 X");
        test.add_line("TEST 54 = X");

        test.nullstep();
        test.nullstep();
        test.debug_state();
    }

    {
        cpu_state test;
        //test.add({"RAND", "0", "15", "X"});
        test.add_line("RAND 0 15 X");

        test.nullstep();

        test.debug_state();
    }

    {
        cpu_state test;
        test.add_line("WARP 123");

        test.nullstep();

        assert(test.ports[(int)hardware::W_DRIVE].value == 123);

        test.debug_state();
    }

    {
        cpu_state test;
        test.add_line("MAKE 1234");
        test.add_line("DROP");

        //test.step();
        //test.step();
    }

    {
        cpu_state test;

        /*MAKE 1234
        TEST EOF
        RSIZ 1
        TEST EOF
        COPY F X0
        TEST EOF
        WIPE*/

        printf("EOF TESTS\n");

        test.add_line("MAKE 1234");
        test.add_line("TEST EOF");
        test.add_line("RSIZ 1");
        test.add_line("TEST EOF");
        test.add_line("COPY F X0");
        test.add_line("TEST EOF");
        test.add_line("WIPE");

        test.nullstep(); //make
        test.nullstep(); //eof

        assert(test.context.register_states[(int)registers::TEST].value == 1);

        test.nullstep(); //rsiz
        test.nullstep(); //test

        assert(test.context.register_states[(int)registers::TEST].value == 0);

        test.nullstep(); //copy
        test.nullstep(); //test

        assert(test.context.register_states[(int)registers::TEST].value == 1);

        test.debug_state();
    }

    ///thanks https://www.reddit.com/r/exapunks/comments/96vox3/how_do_swiz/ reddit beause swiz is a bit
    ///confusing
    {
        cpu_state test;

        test.add_line("SWIZ 8567 3214 X");

        test.nullstep();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == 5678);

        test.add_line("SWIZ 1234 0003 X");
        test.nullstep();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == 2);

        test.add_line("SWIZ 5678 3 X");
        test.nullstep();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == 6);

        test.add_line("SWIZ 5678 32 X");
        test.nullstep();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == 67);


        test.add_line("SWIZ 5678 -32 X");
        test.nullstep();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == -67);
    }

    ///overflow
    {
        cpu_state test;
        test.add_line("ADDI " + std::to_string(std::numeric_limits<int>::max()) + " 1 X0");

        test.nullstep();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == std::numeric_limits<int>::max());
    }

    ///negative overflow
    {
        cpu_state test;
        test.add_line("ADDI " + std::to_string(std::numeric_limits<int>::lowest()) + " -1 X0");

        test.nullstep();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == std::numeric_limits<int>::lowest());
    }

    ///fidx
    {
        cpu_state test;
        test.add_line("MAKE 1234");
        test.add_line("FIDX X0");
        test.add_line("RSIZ 54");
        test.add_line("FIDX X0");
        test.add_line("COPY F X0");
        test.add_line("FIDX X0");
        test.add_line("RSIZ 0");
        test.add_line("FIDX X0");

        register_value& x0 = test.context.register_states[(int)registers::GENERAL_PURPOSE0];

        test.nullstep();
        test.nullstep();

        assert(x0.value == 0);

        test.nullstep();
        test.nullstep();

        assert(x0.value == 0);

        test.nullstep();
        test.nullstep();

        assert(test.files[test.context.held_file].file_pointer == 1);

        assert(x0.value == 1);

        test.nullstep();
        test.nullstep();

        assert(x0.value == 0);
    }

    //exit(0);
}
