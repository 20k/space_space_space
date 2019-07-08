#include "stardust.hpp"
#include "camera.hpp"

stardust_manager::stardust_manager(camera& cam, entity_manager& manage)
{
    std::minstd_rand rng;
    rng.seed(0);

    for(int i=0; i < 1000; i++)
    {
        vec2f min_dist = -(vec2f){100, 100} * 20;
        vec2f max_dist = (vec2f){100, 100} * 20;

        vec2f rpos = rand_det(rng, min_dist, max_dist);
        float z_level = rand_det_s(rng, 1, 5);

        std::shared_ptr<entity> em = manage.make_new<stardust>();

        em->r.position = rpos;
        em->r.init_rectangular({0.1, 0.1});
        em->r.z_level = z_level;
        em->collides = false;
    }


    for(int i=0; i < 200; i++)
    {
        vec2f min_dist = -(vec2f){100, 100} * 20;
        vec2f max_dist = (vec2f){100, 100} * 20;

        vec2f rpos = rand_det(rng, min_dist, max_dist);
        //float z_level = rand_det_s(rng, 1, 5);

        std::shared_ptr<entity> em = manage.make_new<stardust>();

        em->r.position = rpos;
        em->r.init_rectangular({0.1, 0.1});
        em->collides = false;
    }
}
