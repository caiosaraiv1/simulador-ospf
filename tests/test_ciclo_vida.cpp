#include "roteador.hpp"
#include "simulador.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

// ============================================================
// FIXTURE: Rede de 3 nós — nenhuma thread ligada por padrão.
//
//   [1] --10-- [2] --10-- [3]
//    |__________20_________|
// ============================================================
class CicloVidaFixture : public ::testing::Test
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
		adicionar_cabo(r1, r3, 20);
		adicionar_cabo(r3, r1, 20);
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

	void adicionar_cabo(std::shared_ptr<Roteador> origem,
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
// GRUPO 1 — FilaMensagens (sem thread, determinístico)
// ============================================================

// ============================================================
// CASO 1 — wait_pop retorna TIMEOUT com fila vazia
// ============================================================
TEST(FilaMensagensTest, WaitPopRetornaTimeoutComFilaVazia)
{
	FilaMensagens fila;

	auto inicio = std::chrono::steady_clock::now();
	Mensagem msg = fila.wait_pop(std::chrono::milliseconds(100));
	auto fim = std::chrono::steady_clock::now();

	EXPECT_EQ(msg.tipo, TipoMensagem::TIMEOUT);

	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fim - inicio);
	EXPECT_GE(elapsed.count(), 80);
}

// ============================================================
// CASO 2 — wait_pop retorna mensagem antes do timeout
// ============================================================
TEST(FilaMensagensTest, WaitPopRetornaMensagemAntesDoTimeout)
{
	FilaMensagens fila;

	std::thread produtor([&fila]()
	                     {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                Mensagem msg;
                msg.tipo         = TipoMensagem::HELLO;
                msg.remetente_id = "2";
                fila.push_back(msg); });

	Mensagem recebida = fila.wait_pop(std::chrono::milliseconds(500));
	produtor.join();

	EXPECT_EQ(recebida.tipo, TipoMensagem::HELLO);
	EXPECT_EQ(recebida.remetente_id, "2");
}

// ============================================================
// CASO 3 — push_front entrega antes do push_back
//
// Garante que a POISON_PILL tem prioridade sobre mensagens normais.
// ============================================================
TEST(FilaMensagensTest, PushFrontTemPrioridadeSobrePushBack)
{
	FilaMensagens fila;

	Mensagem hello;
	hello.tipo = TipoMensagem::HELLO;
	hello.remetente_id = "2";

	Mensagem pilula;
	pilula.tipo = TipoMensagem::POISON_PILL;

	fila.push_back(hello);
	fila.push_front(pilula);

	Mensagem primeira = fila.wait_pop(std::chrono::milliseconds(100));
	Mensagem segunda = fila.wait_pop(std::chrono::milliseconds(100));

	EXPECT_EQ(primeira.tipo, TipoMensagem::POISON_PILL);
	EXPECT_EQ(segunda.tipo, TipoMensagem::HELLO);
}

// ============================================================
// GRUPO 2 — processar_mensagem() (sem thread, chamada direta)
// ============================================================

// ============================================================
// CASO 4 — LSU atualiza a LSDB local e dispara Dijkstra
//
// r1 recebe LSU de "2" com um link. A tabela de roteamento
// deve registrar "2" como destino roteável.
// ============================================================
TEST_F(CicloVidaFixture, ProcessarLsuAtualizaLsdb)
{
	Link link_de_r2;
	link_de_r2.destino_id = "1";
	link_de_r2.custo = 10;
	link_de_r2.inbox_vizinho = nullptr;

	Mensagem lsu;
	lsu.tipo = TipoMensagem::LSU;
	lsu.remetente_id = "2";
	lsu.payload = {link_de_r2};

	r1->processar_mensagem(lsu);

	auto tabela = r1->get_tabela_roteamento();
	EXPECT_TRUE(tabela.count("2")) << "r1 deveria ter rota para r2 apos processar LSU";
	EXPECT_EQ(tabela.at("2"), "2");
}

// ============================================================
// CASO 5 — LSU duplicado não dispara Dijkstra novamente
//
// Mesmo LSU enviado duas vezes. A tabela de roteamento deve
// ser idêntica após a segunda entrega (LSDB não mudou).
// ============================================================
TEST_F(CicloVidaFixture, ProcessarLsuDuplicadoNaoAlteraLsdb)
{
	Link link_de_r2;
	link_de_r2.destino_id = "1";
	link_de_r2.custo = 10;
	link_de_r2.inbox_vizinho = nullptr;

	Mensagem lsu;
	lsu.tipo = TipoMensagem::LSU;
	lsu.remetente_id = "2";
	lsu.payload = {link_de_r2};

	r1->processar_mensagem(lsu);
	auto tabela_apos_primeiro = r1->get_tabela_roteamento();

	r1->processar_mensagem(lsu);
	auto tabela_apos_segundo = r1->get_tabela_roteamento();

	EXPECT_EQ(tabela_apos_primeiro, tabela_apos_segundo)
	      << "LSU duplicado nao deveria alterar a tabela de roteamento";
}

// ============================================================
// GRUPO 3 — Controle de thread
// ============================================================

// ============================================================
// CASO 6 — POISON_PILL encerra o ciclo mesmo com mensagens na fila
//
// Injeta HELLO na fila antes de desligar. desligar_roteador()
// insere POISON_PILL no front — thread encerra imediatamente.
// ============================================================
TEST_F(CicloVidaFixture, PoisonPillEncerraComMensagensPendentes)
{
	r1->ligar_roteador();

	Mensagem hello;
	hello.tipo = TipoMensagem::HELLO;
	hello.remetente_id = "2";
	r1->get_inbox()->push_back(hello);

	r1->desligar_roteador();

	EXPECT_FALSE(r1->is_ativo());
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
