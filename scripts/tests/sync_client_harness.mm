#include "SyncClient.hpp"
#import <Foundation/Foundation.h>
#include <chrono>
#include <iostream>

int main(int argc, char** argv)
{
    @autoreleasepool
    {
        if (argc != 7)
        {
            return 2;
        }

        SyncClient client(argv[1]);
        SyncClient::Receiver receiver;
        receiver.m_id = "test";
        receiver.m_name = "Loopback";
        receiver.m_host = "127.0.0.1";
        receiver.m_port = std::stoi(argv[2]);
        receiver.m_fingerprint = argv[3];
        client.Start(receiver, argv[4]);
        if (client.Snapshot().m_busy)
        {
            return 4;
        }

        client.Open();
        if (std::string(argv[6]) == "discovery")
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (std::chrono::steady_clock::now() < deadline)
            {
                [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
                for (const auto& found : client.Snapshot().m_receivers)
                {
                    if (found.m_id == "test" && found.m_port == receiver.m_port && found.m_fingerprint == receiver.m_fingerprint)
                    {
                        client.Close();
                        if (!client.Snapshot().m_receivers.empty())
                        {
                            return 7;
                        }

                        std::cout << "discovered" << std::endl;
                        return 0;
                    }
                }
            }

            return 8;
        }

        client.Start(receiver, argv[4]);
        const auto started = std::chrono::steady_clock::now();
        const int cancelMs = std::stoi(argv[5]);
        bool cancelled = false;
        while (client.Snapshot().m_busy)
        {
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
            if (cancelMs >= 0 && elapsed >= cancelMs)
            {
                if (std::string(argv[6]) == "background")
                {
                    client.SetForeground(false);
                    client.Start(receiver, argv[4]);
                    if (client.Snapshot().m_busy)
                    {
                        return 6;
                    }

                    client.SetForeground(true);
                }

                client.Close();
                cancelled = true;
                break;
            }

            if (elapsed > 20000)
            {
                return 3;
            }
        }

        auto status = client.Snapshot();
        client.Close();
        client.SetForeground(true);
        client.Start(receiver, argv[4]);
        if (client.Snapshot().m_busy)
        {
            return 5;
        }

        if (std::string(argv[6]) == "reopen")
        {
            client.Open();
            client.Close();
        }

        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.2]];
        std::cout << (cancelled ? "cancelled" : status.m_message) << std::endl;
        return cancelled || !status.m_failed ? 0 : 1;
    }
}
