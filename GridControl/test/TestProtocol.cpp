#include "../Protocol.hpp"

int main()
{
    Protocol protocol;
    protocol.Connect("127.0.0.1", 7040);

    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            Event event;
            event.m_type = Event::Type::GridColor;
            event.SetGridIndex(i, j);
            event.SetGridColor(255 * static_cast<double>(i) / 9, 255 * static_cast<double>(9 - i) / 9, 255 * static_cast<double>(j) / 8);
            protocol.AddEvent(event);
            event.SetGridIndex(i + 10, j);
            event.SetGridColor(255 * static_cast<double>(9 - i) / 9, 255 * static_cast<double>(j) / 8, 255 * static_cast<double>(i) / 9);
            protocol.AddEvent(event);
        }
    }

    protocol.SendEvents();
    
    return 0;
}
