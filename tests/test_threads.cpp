#include "roteador.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

// ============================================================
// CASO 1 — Roteador inicia ativo e thread sobe sem travar
// ============================================================
TEST(RoteadorThreadTest, LigarIniciaAtivo)
{
	Roteador r("1", nullptr);

	r.ligar_roteador();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_TRUE(r.is_ativo());

	r.desligar_roteador();
}

// ============================================================
// CASO 2 — Após desligar, is_ativo() retorna false
// ============================================================
TEST(RoteadorThreadTest, DesligarEncerraAtivo)
{
	Roteador r("1", nullptr);

	r.ligar_roteador();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	r.desligar_roteador();

	EXPECT_FALSE(r.is_ativo());
}

// ============================================================
// CASO 3 — Múltiplos roteadores rodam em paralelo sem deadlock
// ============================================================
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

// ============================================================
// CASO 4 — ligar -> desligar -> ressucitar não trava nem crasha
//
// ressucitar() é o caminho correto para religar um roteador que
// foi desligado — é o mesmo que rotina_caos() usa internamente.
// ligar_roteador() numa segunda chamada não limpa o estado;
// ressucitar() zera tabela_estados, timers e LSDB aprendida,
// preservando apenas os links locais.
// ============================================================
TEST(RoteadorThreadTest, RessucitarRoteador)
{
	Roteador r("1", nullptr);

	r.ligar_roteador();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	r.desligar_roteador();

	EXPECT_FALSE(r.is_ativo());

	r.ressucitar();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_TRUE(r.is_ativo());

	r.desligar_roteador();
}

// ============================================================
// CASO 5 — ressucitar() limpa o estado aprendido
//
// Injeta links externos na LSDB para simular estado aprendido
// via LSU, depois ressucita. A LSDB do vizinho deve ser zerada
// e apenas os links locais do próprio roteador preservados.
// ============================================================
TEST(RoteadorThreadTest, RessucitarLimpaEstadoAprendido)
{
	Roteador r("1", nullptr);

	// Simula link local de "1"
	Link link_local;
	link_local.destino_id = "2";
	link_local.custo = 10;
	link_local.inbox_vizinho = nullptr;
	r.adicionar_link(link_local);

	// Simula estado aprendido via LSU de "2"
	Link link_externo;
	link_externo.destino_id = "3";
	link_externo.custo = 5;
	link_externo.inbox_vizinho = nullptr;
	r.adicionar_link_na_lsdb("2", link_externo);

	r.ligar_roteador();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	r.desligar_roteador();

	r.ressucitar();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// Após ressucitar, tabela de roteamento deve estar vazia:
	// Dijkstra só roda após novo LSU — estado anterior foi descartado
	auto tabela = r.get_tabela_roteamento();
	EXPECT_TRUE(tabela.empty())
	      << "ressucitar() deveria descartar rotas aprendidas antes da falha";

	r.desligar_roteador();
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
