#pragma once

#include <iostream>
#include <mutex>
#include <string>

class Logger
{
    private:
      // A única fechadura do terminal inteiro
      static std::mutex mtx_terminal;

    public:
      // Método universal que qualquer um pode chamar
      static void imprimir(const std::string& mensagem_formatada);
};
