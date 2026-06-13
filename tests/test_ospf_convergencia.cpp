#include "roteador.hpp"
#include "simulador.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

// ============================================================
// FIXTURE: Rede de 3 nós em linha
//
//   [1] --10-- [2] --10-- [3]
//
// Topologia assimétrica proposital: [1] e [3] não têm link direto,
// portanto [2] é o único caminho entre eles. Isso força o LSU a
// atravessar [2] para que [1] e [3] se conheçam.
// ============================================================
class ConvergenciaFixture : public ::testing::Test
{
    protected:
	Simulador sim;
	std::shared_ptr<Roteador> r1;
	std::shared_ptr<Roteador> r2;
	std::shared_ptr<Roteador> r3;

	void SetUp() override
	{
		r1 = std::make_shared<Roteador>("1", &sim);
		r2 = std::make_shared<Roteador>("2", &sim);
		r3 = std::make_shared<Roteador>("3", &sim);

		sim.registrar_roteador("1", r1);
		sim.registrar_roteador("2", r2);
		sim.registrar_roteador("3", r3);

		// Linha: 1 <-> 2 <-> 3 (sem link direto entre 1 e 3)
		adicionar_cabo(r1, r2, 10);
		adicionar_cabo(r2, r1, 10);
		adicionar_cabo(r2, r3, 10);
		adicionar_cabo(r3, r2, 10);
	}

	void TearDown() override
	{
		if (r1->is_ativo())
			r1->desligar_roteador();
		if (r2->is_ativo())
			r2->desligar_roteador();
		if (r3->is_ativo())
			r3->desligar_roteador();
	}

	void adicionar_cabo(
	      std::shared_ptr<Roteador> origem,
	      std::shared_ptr<Roteador> destino,
	      int custo)
	{
		Link l;
		l.destino_id = destino->get_router_id();
		l.custo = custo;
		l.inbox_vizinho = destino->get_inbox();
		origem->adicionar_link(l);
	}
};

// ============================================================
// CASO 1 — Dois vizinhos diretos convergem para FULL
//
// r1 e r2 trocam HELLOs entre si. Na primeira rodada, cada um
// anuncia lista de vizinhos vazia (DOWN). Na segunda, cada um
// já inclui o outro na lista — bidirecionalidade confirmada,
// estado deve ir para FULL.
//
// Critério de parada: tabela de roteamento de r1 contém "2"
// (Dijkstra só roda após o primeiro LSU, que só sai após FULL).
// ============================================================
TEST_F(ConvergenciaFixture, DoisVizinhosConvergemParaFull)
{
	r1->ligar_roteador();
	r2->ligar_roteador();

	// Dois ciclos de HELLO (2s cada) + margem para processar LSU
	std::this_thread::sleep_for(std::chrono::milliseconds(5500));

	auto tabela_r1 = r1->get_tabela_roteamento();
	auto tabela_r2 = r2->get_tabela_roteamento();

	// Cada roteador deve conhecer o outro como destino roteável
	EXPECT_TRUE(tabela_r1.count("2")) << "r1 nao aprendeu rota para r2";
	EXPECT_TRUE(tabela_r2.count("1")) << "r2 nao aprendeu rota para r1";

	// Next-hop deve ser vizinho direto (único caminho)
	EXPECT_EQ(tabela_r1.at("2"), "2");
	EXPECT_EQ(tabela_r2.at("1"), "1");
}

// ============================================================
// CASO 2 — LSU propaga através de roteador intermediário
//
// Após a rede toda convergir (r1-r2-r3), r1 deve ter rota
// para r3 com next-hop "2" (único caminho possível na topologia
// em linha). Isso prova que o LSU de r3 chegou até r1 via r2.
// ============================================================
TEST_F(ConvergenciaFixture, LsuPropagaPorIntermediario)
{
	r1->ligar_roteador();
	r2->ligar_roteador();
	r3->ligar_roteador();

	// Três ciclos de HELLO para garantir convergência completa na linha
	std::this_thread::sleep_for(std::chrono::milliseconds(7000));

	auto tabela_r1 = r1->get_tabela_roteamento();

	EXPECT_TRUE(tabela_r1.count("3")) << "r1 nao aprendeu rota para r3 via LSU";
	EXPECT_EQ(tabela_r1.at("3"), "2") << "next-hop para r3 deveria ser r2";
}

// ============================================================
// CASO 3 — Estado de vizinho não vai para FULL sem bidirecionalidade
//
// r1 envia HELLO para r2, mas r2 está desligado (inbox não
// consumida). r1 nunca recebe HELLO de volta, portanto nunca
// deve disparar LSU nem preencher a tabela de roteamento.
// ============================================================
TEST_F(ConvergenciaFixture, SemBidirecionaldadeNaoConverge)
{
	// Apenas r1 ligado — r2 não responde
	r1->ligar_roteador();

	std::this_thread::sleep_for(std::chrono::milliseconds(5500));

	auto tabela_r1 = r1->get_tabela_roteamento();

	// Sem HELLO de volta, r1 não deve ter rota para ninguém
	EXPECT_TRUE(tabela_r1.empty())
	      << "r1 nao deveria ter rotas sem confirmar bidirecionalidade";
}

// ============================================================
// CASO 4 — enviar_mensagem_global descarta mensagem se destino inativo
//
// Garante que o Simulador não entrega mensagens a roteadores
// desligados — evita corrida de dados na inbox de um nó morto.
// ============================================================
TEST_F(ConvergenciaFixture, MensagemDescartadaSeDestinoInativo)
{
	// r2 nunca foi ligado — is_ativo() == false

	Mensagem msg;
	msg.tipo = TipoMensagem::HELLO;
	msg.remetente_id = "1";

	// Não deve travar nem crashar
	sim.enviar_mensagem_global("2", msg);

	// Inbox de r2 deve permanecer vazia (mensagem descartada)
	Mensagem recebida = r2->get_inbox()->wait_pop(std::chrono::milliseconds(100));
	EXPECT_EQ(recebida.tipo, TipoMensagem::TIMEOUT)
	      << "inbox de r2 deveria estar vazia; mensagem nao deveria ter sido entregue";
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
