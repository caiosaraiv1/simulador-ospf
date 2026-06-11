#include "roteador.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

// Testa que o roteador inicia ativo e a thread sobe sem travar
TEST(RoteadorThreadTest, LigarIniciaAtivo)
{
      Roteador r("1", nullptr);

      r.ligar_roteador();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      EXPECT_TRUE(r.is_ativo());

      r.desligar_roteador();
}

// Testa que após desligar, is_ativo() retorna false
TEST(RoteadorThreadTest, DesligarEncerraAtivo)
{
      Roteador r("1", nullptr);

      r.ligar_roteador();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      r.desligar_roteador();

      EXPECT_FALSE(r.is_ativo());
}

// Testa que múltiplos roteadores rodam em paralelo sem deadlock
TEST(RoteadorThreadTest, MultiploRoteadoresParalelos)
{
      Roteador r1("1", nullptr);
      Roteador r2("2", nullptr);
      Roteador r3("3", nullptr);

      r1.ligar_roteador();
      r2.ligar_roteador();
      r3.ligar_roteador();

      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      EXPECT_TRUE(r1.is_ativo());
      EXPECT_TRUE(r2.is_ativo());
      EXPECT_TRUE(r3.is_ativo());

      r1.desligar_roteador();
      r2.desligar_roteador();
      r3.desligar_roteador();

      EXPECT_FALSE(r1.is_ativo());
      EXPECT_FALSE(r2.is_ativo());
      EXPECT_FALSE(r3.is_ativo());
}

// Testa que ligar -> desligar -> ligar funciona sem crash
TEST(RoteadorThreadTest, ReligarRoteador)
{
      Roteador r("1", nullptr);

      r.ligar_roteador();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      r.desligar_roteador();

      EXPECT_FALSE(r.is_ativo());

      r.ligar_roteador();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      EXPECT_TRUE(r.is_ativo());

      r.desligar_roteador();
}

int main(int argc, char** argv)
{
      ::testing::InitGoogleTest(&argc, argv);
      return RUN_ALL_TESTS();
}
