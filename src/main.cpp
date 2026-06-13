#include "simulador.hpp"

#include <atomic>
#include <csignal>
#include <iostream>

static std::atomic<bool> rodando{true};

void handler_sinal(int sinal)
{
	(void) sinal;
	rodando = false;
}

int main(int argc, char* argv[])
{
      if (argc < 2)
      {
            std::cerr << "Uso: " << argv[0] << " <caminho_topologia.json>\n";
            return 1;
      }

	std::signal(SIGINT, handler_sinal);

	std::cout << "========================================\n";
	std::cout << "     SimuladorOSPF — Iniciando...\n";
	std::cout << "========================================\n";

	Simulador sim;
	sim.carregar_topologia(argv[1]);

	std::cout << "[MAIN] Topologia carregada. Ligando roteadores...\n\n";

	sim.iniciar_simulacao();

	std::cout << "[MAIN] Rede no ar. Pressione Ctrl+C para encerrar.\n\n";

	while (rodando)
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

	std::cout << "\n[MAIN] Sinal recebido. Encerrando simulação...\n";
	sim.desligar_simulacao();
	std::cout << "[MAIN] Todos os roteadores encerrados.\n";

	return 0;
}
