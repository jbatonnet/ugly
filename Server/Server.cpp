#include "Server.h"
#include <iostream>
#include "../GameServer/GameServerEntryPoint.h"
#include "../GameServer/Game.h"
#include "../ProcessLauncher/ProcessFactory.h"

namespace ugly
{
    namespace server
    {
        ServerArgs ServerArgs::Parse(util::CommandLine& commandLine)
        {
            ServerArgs args;
            commandLine.ConsumeOption(args.gameDll, { "--game", "-game" }, true);
            commandLine.ConsumeOption(args.client, { "--player", "-player" });
            return args;
        }

        class ServerRegistry : public GameServerRegistry
        {
        public:
            void RegisterEntryPoint(loader::unique_ptr<GameServerEntryPoint>&& entryPoint) override
            {
                game = std::move(entryPoint);
            }

            loader::unique_ptr<GameServerEntryPoint> game;
        };

        int Server::Launch(util::CommandLine& commandLine)
        {
            ugly::server::ServerArgs args = ugly::server::ServerArgs::Parse(commandLine);
            commandLine.CheckAllConsummed();
            if (commandLine.HasError())
            {
                std::cerr << commandLine.GetErrorString() << std::endl;
                return 1;
            }
            ServerRegistry registry;
            registry.RegisterLibrary(args.gameDll);
            if (!registry.game)
            {
                std::cerr << "Can't find game " << args.gameDll << std::endl;
                return 2;
            }
            loader::unique_ptr<IGame> game = registry.game->CreateGame(commandLine);
            if (!game)
                return 3;
            for (const std::string& client : args.client)
            {
                loader::unique_ptr<process::IProcess> clientProcess = process::ProcessFactory::CreateProcess(client);
                if (!clientProcess)
                {
                    std::cerr << "Unable to create executable " << client << std::endl;
                    return 4;
                }
                game->AddPlayer(std::move(clientProcess));
            }
            ugly::server::GameResult results = game->Play();
            for (const ugly::server::PlayerResult& player : results.players)
                std::cout << player.rank << ' ' << player.score << ' ';
            std::cout << std::endl;

            return 0;
        }
    }
}

int main(int argc, const char** argv)
{
    ugly::util::CommandLine commandLine(argc, argv);
    return ugly::server::Server::Launch(commandLine);
}