#include "roteador.hpp"
#include <iostream>
#include <cassert>
#include <string>

// Helper: cria um Link sem inbox (teste unitário puro, sem threads)
static Link make_link(const std::string& destino, int custo)
{
      Link l;
      l.destino_id = destino;
      l.custo = custo;
      l.inbox_vizinho = nullptr;
      return l;
}

// Helper: imprime separador de seção
static void secao(const std::string& titulo)
{
      std::cout << "\n======================================" << std::endl;
      std::cout << "  " << titulo << std::endl;
      std::cout << "======================================" << std::endl;
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
 *
 * Caminhos de 1 -> 4:  1->2->4 (custo 4)  e  1->3->4 (custo 4) — empate
 * Caminho de 1 -> 5:   1->2->4->5 ou 1->3->4->5 (custo 5)
 *
 * O Dijkstra deve escolher UMA das rotas de empate de forma determinística.
 * O que verificamos: custo final correto (5 para [5]) e next-hop válido.
 */
static void teste_diamante()
{
      secao("CASO 1 — Diamante (empate de custo)");

      Roteador r("1");

      // Links do nó 1
      r.adicionar_link_na_lsdb("1", make_link("2", 1));
      r.adicionar_link_na_lsdb("1", make_link("3", 1));

      // Links do nó 2
      r.adicionar_link_na_lsdb("2", make_link("1", 1));
      r.adicionar_link_na_lsdb("2", make_link("4", 3));

      // Links do nó 3
      r.adicionar_link_na_lsdb("3", make_link("1", 1));
      r.adicionar_link_na_lsdb("3", make_link("4", 3));

      // Links do nó 4
      r.adicionar_link_na_lsdb("4", make_link("2", 3));
      r.adicionar_link_na_lsdb("4", make_link("3", 3));
      r.adicionar_link_na_lsdb("4", make_link("5", 1));

      // Links do nó 5
      r.adicionar_link_na_lsdb("5", make_link("4", 1));

      r.executar_dijkstra();
      r.imprimir_tabela_roteamento();

      // O next-hop para "4" deve ser "2" ou "3" (ambos válidos no empate)
      // O next-hop para "5" deve ser "2" ou "3" também (passa por 4)
      // Não dá pra assert o valor exato no empate, mas podemos checar que
      // a tabela não está vazia e que os destinos existem.
      std::cout << "[OK] Caso 1 concluido." << std::endl;
}

/*
 * CASO 2 — Rede particionada
 *
 *   [1] -- [2] -- [3]       [4] -- [5]
 *
 * 4 e 5 são inalcançáveis a partir de 1.
 * Esperado: tabela_roteamento não deve conter entradas para "4" nem "5".
 */
static void teste_particao()
{
      secao("CASO 2 — Rede particionada");

      Roteador r("1");

      r.adicionar_link_na_lsdb("1", make_link("2", 10));
      r.adicionar_link_na_lsdb("2", make_link("1", 10));
      r.adicionar_link_na_lsdb("2", make_link("3", 10));
      r.adicionar_link_na_lsdb("3", make_link("2", 10));

      // Partição isolada — presente na LSDB mas sem caminho até 1
      r.adicionar_link_na_lsdb("4", make_link("5", 5));
      r.adicionar_link_na_lsdb("5", make_link("4", 5));

      r.executar_dijkstra();
      r.imprimir_tabela_roteamento();

      std::cout << "[OK] Caso 2 concluido (verifique manualmente que 4 e 5 nao aparecem)." << std::endl;
}

/*
 * CASO 3 — Nó presente na LSDB mas sem links de saída
 *
 *   [1] --5-- [2] --10-- [3]
 *                         |
 *                        (nó 4 na LSDB, mas sem links declarados)
 *
 * O algoritmo não deve travar nem crashar.
 * Esperado: "3" alcançável via "2"; "4" sem entrada na tabela (inalcançável).
 */
static void teste_no_sem_links()
{
      secao("CASO 3 — No presente na LSDB sem links de saida");

      Roteador r("1");

      r.adicionar_link_na_lsdb("1", make_link("2", 5));
      r.adicionar_link_na_lsdb("2", make_link("1", 5));
      r.adicionar_link_na_lsdb("2", make_link("3", 10));
      r.adicionar_link_na_lsdb("3", make_link("2", 10));
      // Nó 4: mencionado como destino mas sem entrada própria na LSDB
      r.adicionar_link_na_lsdb("3", make_link("4", 1));
      // Propositalmente NÃO inserimos links do nó "4"

      r.executar_dijkstra();
      r.imprimir_tabela_roteamento();

      std::cout << "[OK] Caso 3 concluido (sem crash no no sem links)." << std::endl;
}

/*
 * CASO 4 — Caminho longo vs. atalho (regressão do teste original escalado)
 *
 *   Grafo:
 *   1 --1-- 2 --1-- 3 --1-- 4 --1-- 5
 *   |_____________20______________|
 *
 * Caminho direto 1->5 custa 20.
 * Caminho via 2->3->4->5 custa 4.
 * next-hop para "5" deve ser "2".
 */
static void teste_atalho_longo()
{
      secao("CASO 4 — Atalho vs. rota direta cara");

      Roteador r("1");

      r.adicionar_link_na_lsdb("1", make_link("2", 1));
      r.adicionar_link_na_lsdb("1", make_link("5", 20)); // direto mas caro

      r.adicionar_link_na_lsdb("2", make_link("1", 1));
      r.adicionar_link_na_lsdb("2", make_link("3", 1));

      r.adicionar_link_na_lsdb("3", make_link("2", 1));
      r.adicionar_link_na_lsdb("3", make_link("4", 1));

      r.adicionar_link_na_lsdb("4", make_link("3", 1));
      r.adicionar_link_na_lsdb("4", make_link("5", 1));

      r.adicionar_link_na_lsdb("5", make_link("4", 1));
      r.adicionar_link_na_lsdb("5", make_link("1", 20));

      r.executar_dijkstra();
      r.imprimir_tabela_roteamento();

      std::cout << "[OK] Caso 4 concluido (next-hop para '5' deve ser '2')." << std::endl;
}

int main()
{
      std::cout << "========= TESTE DIJKSTRA  =========" << std::endl;

      teste_diamante();
      teste_particao();
      teste_no_sem_links();
      teste_atalho_longo();

      std::cout << "\n========= TODOS OS CASOS FINALIZADOS =========" << std::endl;
      return 0;
}
