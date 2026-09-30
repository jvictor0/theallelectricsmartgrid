#include "doctest.h"
#include "StateSaver.hpp"

DOCTEST_TEST_CASE("StateSaver: scene selectors refresh their side of the blend")
{
    for (float blend : {0.0f, 0.5f, 1.0f})
    {
        DOCTEST_CAPTURE(blend);
        SmartGridOneContext context;
        ScenedStateSaver saver(&context);
        int first = 10;
        int second = 10;
        saver.Insert("first", &first);
        saver.Insert("second", &second);
        saver.Finalize();
        for (State* state : saver.m_state)
        {
            state->Set(20);
            state->CopyToScene(1);
            state->Set(40);
            state->CopyToScene(3);
            state->Set(50);
            state->CopyToScene(4);
            state->Set(10);
        }

        context.m_sceneManager.m_blendFactor = blend;
        saver.Process();
        context.m_sceneManager.m_scene1 = 3;
        saver.Process();
        DOCTEST_CHECK(saver.m_state[0]->Get<int>() == (blend == 0.0f ? 40 : 20));
        DOCTEST_CHECK(saver.m_state[1]->Get<int>() == (blend == 1.0f ? 20 : 40));
        context.m_sceneManager.m_scene2 = 4;
        saver.Process();
        DOCTEST_CHECK(saver.m_state[0]->Get<int>() == (blend == 0.0f ? 40 : 50));
        DOCTEST_CHECK(saver.m_state[1]->Get<int>() == (blend == 1.0f ? 50 : 40));
    }
}
