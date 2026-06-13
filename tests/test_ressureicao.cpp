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
// Mesma topologia do test_ospf_convergencia: [2] é o único
// intermediário entre [1] e [3].
// ============================================================
class RessureicaoFixture : public ::testing::Test
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

	// Aguarda a rede convergir por completo (3 ciclos de HELLO)
	void aguardar_convergencia()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(7000));
	}
};

// ============================================================
// CASO 1 — ressucitar() limpa a tabela de roteamento aprendida
//
// r2 converge com r1 e r3, acumula estado na tabela de
// roteamento. Após ressucitar(), a tabela deve estar vazia —
// o roteador volta ao estado de boot, sem conhecimento prévio.
// ============================================================
TEST_F(RessureicaoFixture, RessucitarLimpaTabelaDeRoteamento)
{
	r1->ligar_roteador();
	r2->ligar_roteador();
	r3->ligar_roteador();

	aguardar_convergencia();

	// Garante que r2 aprendeu rotas antes de ressucitar
	ASSERT_FALSE(r2->get_tabela_roteamento().empty())
	      << "pre-condicao: r2 deveria ter rotas antes de ressucitar";

	r2->desligar_roteador();
	r2->ressucitar();

	// Imediatamente após ressucitar, tabela deve estar zerada
	EXPECT_TRUE(r2->get_tabela_roteamento().empty())
	      << "tabela de roteamento deveria estar vazia apos ressucitar";

	r2->desligar_roteador();
}

// ============================================================
// CASO 2 — ressucitar() preserva os links locais do roteador
//
// Os links físicos (cabos) não mudam quando um roteador reinicia.
// Após ressucitar(), r2 deve continuar com seus dois links
// configurados (para r1 e para r3), sem precisar recabeamento.
// ============================================================
TEST_F(RessureicaoFixture, RessucitarPreservaLinksLocais)
{
	r1->ligar_roteador();
	r2->ligar_roteador();
	r3->ligar_roteador();

	aguardar_convergencia();

	r2->desligar_roteador();
	r2->ressucitar();

	// Verifica que r2 ainda consegue enviar HELLOs para r1 e r3
	// indiretamente: após novo ciclo de convergência, r1 deve
	// receber HELLO de r2, o que só é possível se r2 tem o link
	Mensagem recebida = r1->get_inbox()->wait_pop(std::chrono::milliseconds(3000));
	EXPECT_EQ(recebida.tipo, TipoMensagem::HELLO)
	      << "r1 deveria receber HELLO de r2 ressuscitado (link local preservado)";
	EXPECT_EQ(recebida.remetente_id, "2");

	r2->desligar_roteador();
}

// ============================================================
// CASO 3 — roteador ressuscitado re-converge com a rede
//
// Após ser derrubado e ressucitado, r2 deve completar um novo
// ciclo de convergência com r1 e r3, reconstruindo as rotas
// do zero via troca de HELLOs e LSUs.
//
// Usa polling ativo em vez de sleep fixo porque ressucitar()
// faz join() na thread anterior — o tempo real até a nova
// thread estar ativa e processar HELLOs é imprevisível.
// ============================================================
TEST_F(RessureicaoFixture, RessucitadoReconvergeComRede)
{
	r1->ligar_roteador();
	r2->ligar_roteador();
	r3->ligar_roteador();

	aguardar_convergencia();

	r2->desligar_roteador();
	r2->ressucitar();

	// Polling: verifica a cada 500ms se r2 já aprendeu as duas rotas.
	// Deadline de 30s para evitar loop infinito em caso de regressão.
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
	bool convergiu = false;

	while (std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		auto tabela = r2->get_tabela_roteamento();
		if (tabela.count("1") && tabela.count("3"))
		{
			convergiu = true;
			break;
		}
	}

	ASSERT_TRUE(convergiu)
	      << "r2 ressuscitado nao reconvergiu dentro do prazo de 30s";

	auto tabela_r2 = r2->get_tabela_roteamento();
	EXPECT_EQ(tabela_r2.at("1"), "1");
	EXPECT_EQ(tabela_r2.at("3"), "3");

	r2->desligar_roteador();
}

// ============================================================
// CASO 4 — inbox é drenada antes de ressucitar
//
// Mensagens enfileiradas enquanto o roteador estava morto não
// devem ser processadas após a ressureição — o roteador volta
// do zero, sem processar "fantasmas" da vida anterior.
// ============================================================
TEST_F(RessureicaoFixture, InboxDrenadaAoRessucitar)
{
	r2->ligar_roteador();
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	r2->desligar_roteador();

	// Injeta mensagem "fantasma" enquanto r2 está morto
	Mensagem fantasma;
	fantasma.tipo = TipoMensagem::LSU;
	fantasma.remetente_id = "99";
	r2->get_inbox()->push_back(fantasma);

	r2->ressucitar();

	// Inbox deve ter sido drenada; a próxima mensagem da fila
	// deve ser TIMEOUT (fila vazia), não o LSU fantasma
	Mensagem recebida = r2->get_inbox()->wait_pop(std::chrono::milliseconds(100));
	EXPECT_EQ(recebida.tipo, TipoMensagem::TIMEOUT)
	      << "inbox deveria estar vazia apos ressucitar; LSU fantasma nao deveria estar la";

	r2->desligar_roteador();
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
