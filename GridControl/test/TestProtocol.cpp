#include "../Protocol.hpp"

int main()
{
    Protocol protocol;
    protocol.Connect("127.0.0.1", 7040);
    protocol.Handshake(1);

    bool on[20][8];

    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            on[i][j] = true;
            on[i + 10][j] = true;
            Event event;
            event.m_type = Event::Type::GridColor;
            event.SetGridIndex(i, j);
            event.SetGridColor(255 * static_cast<double>(i) / 9, 255 * static_cast<double>(9 - i) / 9, 255 * static_cast<double>(j) / 8);
            protocol.AddEvent(event);
            TestMidiRoundTrip(event);
            event.SetGridIndex(i + 10, j);
            event.SetGridColor(255 * static_cast<double>(9 - i) / 9, 255 * static_cast<double>(j) / 8, 255 * static_cast<double>(i) / 9);
            protocol.AddEvent(event);
            TestMidiRoundTrip(event);
        }
    }

    protocol.SendEvents();

    while (true)
    {
        std::vector<Event> events;
        protocol.GetEvents(events);
        for (const Event &event : events)
        {
            TestMidiRoundTrip(event);
            if (event.m_type == Event::Type::GridTouch && event.GetVelocity() > 0)
            {
                int i = event.GetX();
                int j = event.GetY();
                on[i][j] = !on[i][j];
                Event responseEvent;
                responseEvent.m_type = Event::Type::GridColor;
                responseEvent.SetGridIndex(i, j);
                if (on[i][j])
                {
                    if (i < 10)
                    {
                        responseEvent.SetGridColor(255 * static_cast<double>(i) / 9, 255 * static_cast<double>(9 - i) / 9, 255 * static_cast<double>(j) / 8);
                    }
                    else
                    {
                        responseEvent.SetGridColor(255 * static_cast<double>(9 - (i - 10)) / 9, 255 * static_cast<double>(j) / 8, 255 * static_cast<double>(i - 10) / 9);
                    }
                }
                else
                {
                    responseEvent.SetGridColor(0, 0, 0);
                }

                protocol.AddEvent(responseEvent);
                TestMidiRoundTrip(responseEvent);
            }
        }

        protocol.SendEvents();
        sleep(0.02);
    }           
    
    return 0;
}
