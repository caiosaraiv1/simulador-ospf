#include "roteador.hpp"
#include <gtest/gtest.h>
#include <string>

static Link make_link(const std::string& destino, int custo)
{
      Link l;
      l.destino_id = destino;
      l.custo = custo;
      l.inbox_vizinho = nullptr;
      return l;
}

/*
 * CASO 1 — Topologia em diamante (5 nós)
 *
 *         [1]
 *        /   \
 *      1       1
 *      /         \
 *    [2]         [3]
 *      \         /
 *      3       3
 *        \   /
 *         [4]
 *          |
 *          1
 *         [5]
 */
TEST(DijkstraTest, Diamante)
{
      Roteador r("1");

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

      // Empate: next-hop para "4" e "5" pode ser "2" ou "3", ambos válidos
      auto tabela = r.get_tabela_roteamento();
      EXPECT_TRUE(tabela.count("2"));
      EXPECT_TRUE(tabela.count("3"));
      EXPECT_TRUE(tabela.count("4"));
      EXPECT_TRUE(tabela.count("5"));

      std::string hop_4 = tabela.at("4");
      EXPECT_TRUE(hop_4 == "2" || hop_4 == "3");

      std::string hop_5 = tabela.at("5");
      EXPECT_TRUE(hop_5 == "2" || hop_5 == "3");
}

/*
 * CASO 2 — Rede particionada
 *
 *   [1] -- [2] -- [3]       [4] -- [5]
 */
TEST(DijkstraTest, RedeParticionada)
{
      Roteador r("1");

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
}

/*
 * CASO 3 — Nó presente na LSDB mas sem links de saída
 *
 *   [1] --5-- [2] --10-- [3] --1-- [4 sem links]
 */
TEST(DijkstraTest, NoSemLinks)
{
      Roteador r("1");

      r.adicionar_link_na_lsdb("1", make_link("2", 5));
      r.adicionar_link_na_lsdb("2", make_link("1", 5));
      r.adicionar_link_na_lsdb("2", make_link("3", 10));
      r.adicionar_link_na_lsdb("3", make_link("2", 10));
      r.adicionar_link_na_lsdb("3", make_link("4", 1));
      // Nó "4": mencionado como destino mas sem entrada própria na LSDB

      r.executar_dijkstra();

      auto tabela = r.get_tabela_roteamento();
      EXPECT_TRUE(tabela.count("2"));
      EXPECT_TRUE(tabela.count("3"));
      EXPECT_EQ(tabela.at("2"), "2");
      EXPECT_EQ(tabela.at("3"), "2");
      EXPECT_FALSE(tabela.count("4")); // Sem links de saída, não aparece
}

/*
 * CASO 4 — Caminho longo vs. atalho
 *
 *   1 --1-- 2 --1-- 3 --1-- 4 --1-- 5
 *   |___________20___________________|
 *
 * Direto 1->5 custa 20. Via 2->3->4->5 custa 4.
 */
TEST(DijkstraTest, AtalhoVsRotaDiretaCara)
{
      Roteador r("1");

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
      EXPECT_EQ(tabela.at("5"), "2"); // Deve ignorar o direto caro
}

int main(int argc, char** argv)
{
      ::testing::InitGoogleTest(&argc, argv);
      return RUN_ALL_TESTS();
}
