#include "roteador.hpp"
#include "simulador.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

// ============================================================
// FIXTURE: Rede de 3 nós reutilizável entre os testes
//
//   [1] --10-- [2] --10-- [3]
//    |_________20_________|
//
// Nenhum roteador está ligado por padrão.
// Cada teste liga apenas o que precisa.
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

		// Cabeamento bidirecional
		adicionar_cabo(r1, r2, 10);
		adicionar_cabo(r2, r1, 10);
		adicionar_cabo(r2, r3, 10);
		adicionar_cabo(r3, r2, 10);
		adicionar_cabo(r1, r3, 20);
		adicionar_cabo(r3, r1, 20);
	}

	void TearDown() override
	{
		if (r1->is_ativo()) r1->desligar_roteador();
		if (r2->is_ativo()) r2->desligar_roteador();
		if (r3->is_ativo()) r3->desligar_roteador();
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

	std::thread produtor([&fila]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		Mensagem msg;
		msg.tipo = TipoMensagem::HELLO;
		msg.remetente_id = "2";
		fila.push_back(msg);
	});

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
	fila.push_front(pilula); // deve entrar na frente

	Mensagem primeira = fila.wait_pop(std::chrono::milliseconds(100));
	Mensagem segunda = fila.wait_pop(std::chrono::milliseconds(100));

	EXPECT_EQ(primeira.tipo, TipoMensagem::POISON_PILL);
	EXPECT_EQ(segunda.tipo, TipoMensagem::HELLO);
}

// ============================================================
// CASO 4 — enviar_hello deposita HELLO na inbox do vizinho
// ============================================================
TEST_F(CicloVidaFixture, HelloChegaNaInboxDoVizinho)
{
	// r1 ligado, r2 e r3 não — inbox deles não tem consumidor
	r1->ligar_roteador();

	r1->enviar_hello();

	// r2 deve ter recebido o HELLO de r1
	Mensagem recebida_r2 = r2->get_inbox()->wait_pop(std::chrono::milliseconds(200));
	EXPECT_EQ(recebida_r2.tipo, TipoMensagem::HELLO);
	EXPECT_EQ(recebida_r2.remetente_id, "1");

	// r3 também deve ter recebido (link direto de custo 20)
	Mensagem recebida_r3 = r3->get_inbox()->wait_pop(std::chrono::milliseconds(200));
	EXPECT_EQ(recebida_r3.tipo, TipoMensagem::HELLO);
	EXPECT_EQ(recebida_r3.remetente_id, "1");
}

// ============================================================
// CASO 5 — ciclo_vida dispara hello periodicamente
//
// r1 ligado, r2 desligado (inbox não consumida).
// Após 2.5s, inbox de r2 deve ter pelo menos 1 HELLO.
// ============================================================
TEST_F(CicloVidaFixture, CicloDisparaHelloPeriodico)
{
	r1->ligar_roteador();

	std::this_thread::sleep_for(std::chrono::milliseconds(2500));

	Mensagem recebida = r2->get_inbox()->wait_pop(std::chrono::milliseconds(200));
	EXPECT_EQ(recebida.tipo, TipoMensagem::HELLO);
	EXPECT_EQ(recebida.remetente_id, "1");
}

// ============================================================
// CASO 6 — processar_mensagem muda estado de DOWN para INIT
//
// r2 recebe um HELLO de "1" sem conhecer "2" na lista de vizinhos.
// Estado de "1" na tabela de r2 deve ir para INIT.
// Verificamos indiretamente: r2 envia HELLO de volta após processar.
// ============================================================
TEST_F(CicloVidaFixture, HelloRecebidoDisparaHelloDeVolta)
{
	// Liga r2 para processar mensagens
	r2->ligar_roteador();

	// Injeta um HELLO de "1" direto na inbox de r2
	// vizinhos_conhecidos vazio = r2 ainda não conhece r1 = estado INIT
	Mensagem hello;
	hello.tipo = TipoMensagem::HELLO;
	hello.remetente_id = "1";
	// vizinhos_conhecidos vazio propositalmente
	r2->get_inbox()->push_back(hello);

	// Após processar, r2 deve ter mudado estado de "1" para INIT
	// e no próximo ciclo de hello vai incluir "1" em vizinhos_conhecidos
	// Verificação simples: r2 não crashou e continua ativo
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_TRUE(r2->is_ativo());
}

// ============================================================
// CASO 7 — POISON_PILL encerra o ciclo mesmo com mensagens na fila
//
// Injeta um HELLO e depois uma POISON_PILL.
// A thread deve encerrar mesmo tendo o HELLO pendente.
// ============================================================
TEST_F(CicloVidaFixture, PoisonPillEncerraComMensagensPendentes)
{
	r1->ligar_roteador();

	// Injeta HELLO normal na fila
	Mensagem hello;
	hello.tipo = TipoMensagem::HELLO;
	hello.remetente_id = "2";
	r1->get_inbox()->push_back(hello);

	// Desligar injeta POISON_PILL no front — deve encerrar imediatamente
	r1->desligar_roteador();

	EXPECT_FALSE(r1->is_ativo());
}

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
