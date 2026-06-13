#include "roteador.hpp"

#include <gtest/gtest.h>
#include <string>

// ─── Fixture ──────────────────────────────────────────────────────────────────

class DijkstraFixture : public ::testing::Test
{
    protected:
	static Link make_link(const std::string &destino, int custo)
	{
		Link l;
		l.destino_id = destino;
		l.custo = custo;
		l.inbox_vizinho = nullptr;
		return l;
	}
};

/* ============================================================
// CASO 1 — Topologia em diamante (5 nós)
//
//         [1]
//        /   \
//      1       1
//      /         \
//    [2]         [3]
//      \         /
//      3       3
//        \   /
//         [4]
//          |
//          1
//         [5]
//
// Empate: next-hop para "4" e "5" pode ser "2" ou "3" — ambos
// custam o mesmo. Qualquer um dos dois é correto.
// ============================================================*/
TEST_F(DijkstraFixture, Diamante)
{
	Roteador r("1", nullptr);

	r.adicionar_link_na_lsdb("1", make_link("2", 1));
	r.adicionar_link_na_lsdb("1", make_link("3", 1));
	r.adicionar_link_na_lsdb("2", make_link("1", 1));
	r.adicionar_link_na_lsdb("2", make_link("4", 3));
	r.adicionar_link_na_lsdb("3", make_link("1", 1));
	r.adicionar_link_na_lsdb("3", make_link("4", 3));
	r.adicionar_link_na_lsdb("4", make_link("2", 3));
	r.adicionar_link_na_lsdb("4", make_link("3", 3));
	r.adicionar_link_na_lsdb("4", make_link("5", 1));
	r.adicionar_link_na_lsdb("5", make_link("4", 1));

	r.executar_dijkstra();

	auto tabela = r.get_tabela_roteamento();

	EXPECT_TRUE(tabela.count("2"));
	EXPECT_TRUE(tabela.count("3"));
	EXPECT_TRUE(tabela.count("4"));
	EXPECT_TRUE(tabela.count("5"));

	std::string hop_4 = tabela.at("4");
	EXPECT_TRUE(hop_4 == "2" || hop_4 == "3");

	std::string hop_5 = tabela.at("5");
	EXPECT_TRUE(hop_5 == "2" || hop_5 == "3");

	// Origem nunca deve aparecer como destino na própria tabela
	EXPECT_FALSE(tabela.count("1"));
}

// ============================================================
// CASO 2 — Rede particionada
//
//   [1] -- [2] -- [3]       [4] -- [5]
//
// "4" e "5" existem na LSDB mas são inalcançáveis a partir de
// "1" — não devem aparecer na tabela de roteamento.
// ============================================================
TEST_F(DijkstraFixture, RedeParticionada)
{
	Roteador r("1", nullptr);

	r.adicionar_link_na_lsdb("1", make_link("2", 10));
	r.adicionar_link_na_lsdb("2", make_link("1", 10));
	r.adicionar_link_na_lsdb("2", make_link("3", 10));
	r.adicionar_link_na_lsdb("3", make_link("2", 10));
	r.adicionar_link_na_lsdb("4", make_link("5", 5));
	r.adicionar_link_na_lsdb("5", make_link("4", 5));

	r.executar_dijkstra();

	auto tabela = r.get_tabela_roteamento();

	EXPECT_TRUE(tabela.count("2"));
	EXPECT_TRUE(tabela.count("3"));
	EXPECT_FALSE(tabela.count("4")); // Inalcançável
	EXPECT_FALSE(tabela.count("5")); // Inalcançável
	EXPECT_FALSE(tabela.count("1")); // Origem nunca é destino
}

// ============================================================
// CASO 3 — Nó presente na LSDB mas sem links de saída
//
//   [1] --5-- [2] --10-- [3] --1-- [4 sem links]
//
// "4" é mencionado como destino de "3" mas não tem entrada
// própria na LSDB. O Dijkstra trata o bloco de "4" como vazio
// e o ignora — não deve aparecer na tabela.
// ============================================================
TEST_F(DijkstraFixture, NoSemLinks)
{
	Roteador r("1", nullptr);

	r.adicionar_link_na_lsdb("1", make_link("2", 5));
	r.adicionar_link_na_lsdb("2", make_link("1", 5));
	r.adicionar_link_na_lsdb("2", make_link("3", 10));
	r.adicionar_link_na_lsdb("3", make_link("2", 10));
	r.adicionar_link_na_lsdb("3", make_link("4", 1));
	// "4": mencionado como destino mas sem entrada própria na LSDB

	r.executar_dijkstra();

	auto tabela = r.get_tabela_roteamento();

	EXPECT_TRUE(tabela.count("2"));
	EXPECT_TRUE(tabela.count("3"));
	EXPECT_EQ(tabela.at("2"), "2");
	EXPECT_EQ(tabela.at("3"), "2");
	EXPECT_FALSE(tabela.count("4")); // Sem links de saída, não aparece
	EXPECT_FALSE(tabela.count("1")); // Origem nunca é destino
}

// ============================================================
// CASO 4 — Caminho longo vs. atalho
//
//   [1] --1-- [2] --1-- [3] --1-- [4] --1-- [5]
//    |__________________20___________________|
//
// Link direto 1->5 custa 20. Caminho via 2->3->4->5 custa 4.
// O Dijkstra deve preferir o caminho mais barato e ignorar o
// link direto caro.
// ============================================================
TEST_F(DijkstraFixture, AtalhoVsRotaDiretaCara)
{
	Roteador r("1", nullptr);

	r.adicionar_link_na_lsdb("1", make_link("2", 1));
	r.adicionar_link_na_lsdb("1", make_link("5", 20));
	r.adicionar_link_na_lsdb("2", make_link("1", 1));
	r.adicionar_link_na_lsdb("2", make_link("3", 1));
	r.adicionar_link_na_lsdb("3", make_link("2", 1));
	r.adicionar_link_na_lsdb("3", make_link("4", 1));
	r.adicionar_link_na_lsdb("4", make_link("3", 1));
	r.adicionar_link_na_lsdb("4", make_link("5", 1));
	r.adicionar_link_na_lsdb("5", make_link("4", 1));
	r.adicionar_link_na_lsdb("5", make_link("1", 20));

	r.executar_dijkstra();

	auto tabela = r.get_tabela_roteamento();

	EXPECT_EQ(tabela.at("5"), "2");  // Deve ignorar o link direto caro
	EXPECT_FALSE(tabela.count("1")); // Origem nunca é destino
}

// ============================================================
// CASO 5 — Nó isolado: LSDB só com o próprio roteador
//
// Roteador sem vizinhos na LSDB. A tabela de roteamento deve
// ficar completamente vazia — nenhum destino alcançável.
// ============================================================
TEST_F(DijkstraFixture, NoIsolado)
{
	Roteador r("1", nullptr);

	// Nenhum link adicionado — LSDB vazia
	r.executar_dijkstra();

	auto tabela = r.get_tabela_roteamento();

	EXPECT_TRUE(tabela.empty());
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
