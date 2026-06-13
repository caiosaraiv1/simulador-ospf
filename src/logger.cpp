#include "logger.hpp"

#include <string>
#include <thread>

static std::mutex mtx_log;

void Logger::imprimir(const std::string &mensagem_formatada)
{
	std::scoped_lock lock(mtx_log);
	std::cout << mensagem_formatada;
}
