# SimuladorOSPF

Simulador do protocolo de roteamento **OSPF (Open Shortest Path First)** implementado em C++20, com foco em concorrência real, injeção de falhas e convergência dinâmica de rede. Desenvolvido como projeto de estudo para sistemas distribuídos.

---

## O que é OSPF?

OSPF é um protocolo de roteamento de estado de enlace (*link-state*) amplamente utilizado em redes corporativas e de backbone. Diferente de protocolos de vetor de distância (como RIP), cada roteador no OSPF mantém uma visão completa da topologia da rede — a **LSDB (Link State Database)** — e calcula independentemente o menor caminho para cada destino usando o algoritmo de Dijkstra.

O protocolo opera em três fases fundamentais:

1. **Descoberta de vizinhos** via pacotes HELLO periódicos
2. **Inundação de estado de enlace** via LSUs (Link State Updates) para disseminar a topologia
3. **Cálculo de rotas** via SPF (Shortest Path First / Dijkstra)

---

## Esta implementação vs. OSPF real (RFC 2328)

Esta é uma implementação educacional que captura a essência do protocolo, com simplificações deliberadas:

| Aspecto | OSPF Real (RFC 2328) | Esta implementação |
|---|---|---|
| **Transporte** | IP/UDP na rede física | Chamada direta à inbox em memória |
| **Estados de adjacência** | DOWN → INIT → 2-WAY → EXSTART → EXCHANGE → LOADING → FULL | DOWN → INIT → FULL |
| **DR/BDR** | Eleição de Designated Router em redes broadcast | Ausente — flooding direto para todos os vizinhos FULL |
| **LSA types** | Router LSA, Network LSA, Summary LSA, AS External LSA | Único tipo: links locais do roteador |
| **Sequência de LSA** | Número de sequência para controle de versão | Comparação por igualdade de payload |
| **Hello/Dead interval** | Configurável por interface | Hello = 2s, Dead = 5s (fixo) |
| **Áreas OSPF** | Área backbone + áreas stub, NSSA, etc. | Rede flat (área única) |
| **Autenticação** | MD5 / SHA-HMAC | Ausente |
| **Retransmissão** | ACK e retransmissão confiável de LSAs | Ausente — canal de entrega confiável por design |

A transição INIT → FULL implementa corretamente a **verificação de bidirecionalidade**: um roteador só sobe para FULL quando recebe um HELLO do vizinho que o lista explicitamente na sua lista de vizinhos conhecidos — fiel à RFC.

---

## Arquitetura

```
┌─────────────────────────────────────────────────────┐
│                    Simulador                        │
│                                                     │
│  rede: unordered_map<id, shared_ptr<Roteador>>      │
│  thread_caos ──► rotina_caos()                      │
│  enviar_mensagem_global()                           │
└──────────────┬──────────────────────────────────────┘
               │ shared_ptr
       ┌───────┴────────┐
       ▼                ▼
  ┌─────────┐      ┌─────────┐      ...
  │Roteador │      │Roteador │
  │         │      │         │
  │ lsdb    │      │ lsdb    │
  │ tabela_ │      │ tabela_ │
  │ estados │      │ estados │
  │ tabela_ │      │ tabela_ │
  │ rotea.. │      │ rotea.. │
  │         │      │         │
  │ inbox ◄─┼──────┼── msgs  │
  │ thread_ │      │ thread_ │
  │ trabalho│      │ trabalho│
  └─────────┘      └─────────┘
```

### Componentes

**`Simulador`** — orquestrador central. Carrega a topologia do JSON, instancia os roteadores, dispara as threads e opera a `rotina_caos` em thread dedicada. É o barramento de mensagens: `enviar_mensagem_global()` entrega diretamente na inbox do destinatário, descartando silenciosamente se o roteador estiver inativo.

**`Roteador`** — entidade autônoma. Cada instância roda em sua própria `std::thread` (thread de trabalho), gerencia seu próprio estado OSPF e nunca compartilha dados mutáveis com outros roteadores — toda comunicação é por troca de mensagens.

**`FilaMensagens`** — fila thread-safe com modelo produtor/consumidor. Baseada em `std::deque` + `std::mutex` + `std::condition_variable`. Suporta `wait_pop` com timeout (para o ciclo de HELLO) e `push_front` prioritário para a Poison Pill.

**`Logger`** — singleton de impressão thread-safe. Um único mutex global garante que logs de múltiplas threads não se entrelacem no terminal.

---

## Fluxo de funcionamento

### 1. Boot e descoberta de vizinhos

```
Roteador A                          Roteador B
    │                                   │
    ├──── HELLO (vizinhos: []) ─────────►│  A vê B pela 1ª vez → B entra em INIT
    │◄─── HELLO (vizinhos: []) ──────────┤  B vê A pela 1ª vez → A entra em INIT
    │                                   │
    ├──── HELLO (vizinhos: [B]) ────────►│  B vê seu ID na lista → B vai para FULL
    │◄─── HELLO (vizinhos: [A]) ──────────┤  A vê seu ID na lista → A vai para FULL
    │                                   │
    ├──── LSU (links de A) ─────────────►│  B atualiza LSDB → roda Dijkstra
    │◄─── LSU (links de B) ──────────────┤  A atualiza LSDB → roda Dijkstra
```

### 2. Propagação de estado (flooding)

Quando um roteador recebe um LSU com payload diferente do que já tem na LSDB, ele atualiza o banco, recalcula rotas e re-encaminha o LSU para todos os vizinhos FULL exceto o remetente original. Isso garante que a topologia completa chegue a todos os nós sem loops de flooding.

### 3. Detecção de falha (Dead Timer)

O ciclo de vida usa `wait_pop` com timeout dinâmico calculado para disparar exatamente no intervalo do HELLO (2s). Após cada mensagem processada, verifica o dead timer de cada vizinho ativo: se o último HELLO foi há mais de 5s, o vizinho vai para DOWN e um novo LSU é inundado anunciando a perda.

### 4. Encerramento limpo (Poison Pill)

`desligar_roteador()` usa o padrão **Poison Pill**: injeta uma mensagem especial no *front* da fila (com `push_front`, prioridade máxima) que faz a thread de trabalho sair do loop sem busy-wait. Depois faz `thread.join()` — encerramento determinístico, sem race condition.

### 5. Ressureição

`ressucitar()` limpa toda a tabela de estados, timers, LSDB aprendida e inbox — o roteador volta ao estado de boot. Os **links locais são preservados** (representam o cabeamento físico, que não muda com uma falha de software). Uma nova thread de trabalho é criada do zero.

### 6. Cálculo de rotas (Dijkstra / SPF)

Sempre que a LSDB muda — por recebimento de um LSU novo — o roteador dispara `executar_dijkstra()` sobre o grafo completo que tem em memória. O algoritmo mantém uma tabela de distâncias inicializada com `∞` para todos os nós exceto o próprio roteador (custo 0), e a cada iteração escolhe o nó não visitado com menor distância acumulada para relaxar suas arestas. Ao final, reconstrói a tabela de roteamento caminhando de volta no mapa de predecessores para encontrar o **next-hop** de cada destino — o vizinho direto pelo qual o menor caminho passa.

Nós inalcançáveis (partições de rede) ficam com distância `∞` e nunca entram na tabela. Nós conhecidos na LSDB mas sem entrada própria (mencionados como destino mas sem links de saída) são ignorados silenciosamente.

### 7. Injeção de caos

A `rotina_caos` roda em thread separada com ciclos de 6s:
- **40% de chance** de derrubar um roteador ativo aleatório
- **30% de chance** de ressuscitar um roteador elegível (morto há ≥ 30s)

Isso força a rede a convergir continuamente, testando a resiliência do protocolo.

---

## Stack tecnológica

| Ferramenta | Papel |
|---|---|
| **C++20** | Linguagem principal; uso de `std::ranges`, `std::scoped_lock`, `[[nodiscard]]` |
| **CMake 3.x** | Build system |
| **Ninja** | Backend de compilação — substituição ao Make, builds paralelos mais rápidos |
| **Conan 2** | Gerenciador de dependências |
| **nlohmann/json** | Parse do arquivo de topologia |
| **GoogleTest** | Framework de testes unitários e de integração |
| **Docker** | Ambiente de build reprodutível (multi-stage) |

---

## Topologias disponíveis

Os arquivos em `data/` descrevem grafos dirigidos e ponderados em JSON. Os IDs dos roteadores seguem o formato de endereço IP (`1.1.1.1`, `2.2.2.2`, ...) para se aproximar da notação real do OSPF.

| Arquivo | Nós | Tipo | Característica |
|---|---|---|---|
| `standard.json` | 5 | Malha pequena | Referência para desenvolvimento; topologia mínima com caminhos alternativos |
| `15_grid.json` | 15 | Grade parcial | Cada nó conectado a ~3 vizinhos; boa para observar flooding em cascata |
| `15_mesh.json` | 15 | Malha parcial | Conectividade média; múltiplos caminhos entre a maioria dos pares |
| `15_ring_stress.json` | 15 | Anel | Topologia linear fechada; stress de convergência quando um nó cai |
| `25_mesh_denso.json` | 25 | Malha densa | Cada nó com ~8 vizinhos; convergência rápida, alto volume de LSUs |
| `25_random_sparse.json` | 25 | Grafo esparso | Alguns nós com grau 1; testa particionamento e isolamento |
| `30_mesh.json` | 30 | Malha média | Cada nó conectado a ~8 vizinhos em janela deslizante |
| `50_grid.json` | 50 | Grade 2×camadas | Estrutura em duas fileiras; cada nó com 3-4 vizinhos |
| `50_mesh_stress.json` | 50 | Malha densa | Cada nó com ~8 vizinhos; stress máximo de threads e flooding |

### Formato do JSON de topologia

```json
{
  "routers": [
    {
      "id": "1.1.1.1",
      "links": [
        { "destino_id": "2.2.2.2", "custo": 5 },
        { "destino_id": "3.3.3.3", "custo": 10 }
      ]
    },
    {
      "id": "2.2.2.2",
      "links": [
        { "destino_id": "1.1.1.1", "custo": 5 }
      ]
    }
  ]
}
```

Links são **direcionados** — para uma conexão bidirecional, ambos os lados precisam declarar o link. Custos são inteiros positivos e representam o peso da aresta no Dijkstra (equivalente ao OSPF cost baseado em bandwidth). Os custos podem ser **assimétricos**: `1.1.1.1 → 2.2.2.2` com custo 5 e `2.2.2.2 → 1.1.1.1` com custo 13 são situações válidas e comuns nos arquivos de exemplo.

---

## Como rodar

A forma recomendada é via Docker — não exige instalar CMake, Conan, Ninja ou GCC na sua máquina. O `Dockerfile` usa multi-stage build: a imagem `builder` compila tudo e a imagem final copia apenas o binário e os dados.

### Docker (recomendado)

```bash
# Build da imagem (compilação completa dentro do container)
docker build -t simulador-ospf .

# Rodar com a topologia padrão (5 roteadores)
docker run -it --rm simulador-ospf

# Rodar com outra topologia
docker run -it --rm simulador-ospf data/50_mesh_stress.json

# Encerrar com Ctrl+C — o SIGINT é propagado e o shutdown é limpo
```

> O `-it` é necessário para que o `Ctrl+C` propague o SIGINT corretamente ao processo dentro do container. Sem ele, o sinal não chega ao binário e o container precisa ser encerrado à força.
>
> O `CMD` padrão do Dockerfile já aponta para `data/standard.json`, então `docker run -it --rm simulador-ospf` funciona sem argumentos.

### Exemplo de saída

```
========================================
     SimuladorOSPF — Iniciando...
========================================
[MAIN] Topologia carregada. Ligando roteadores...

[MAIN] Rede no ar. Pressione Ctrl+C para encerrar.

[00:00:02] 1.1.1.1 %%01OSPF/6/NBR_CHG: Neighbor 2.2.2.2 state changed to INIT
[00:00:04] 1.1.1.1 %%01OSPF/5/NHB_CHG: Neighbor 2.2.2.2 state changed to FULL
[00:00:04] 1.1.1.1 %%01OSPF/6/LSU_TX: Flooding local LSDB updates to adjacent neighbors
[00:00:04] 2.2.2.2 %%01OSPF/6/LSDB_UPD: Received newer LSDB update from 1.1.1.1. Updating local database.
[00:00:04] 2.2.2.2 %%01OSPF/6/SPF_RUN: LSDB changed. Triggering shortest path first (Dijkstra) calculation.
[00:00:12] SIMULATOR %%CHAOS/1/KILL: Injecting failure. Powering off router 3.3.3.3
[00:00:14] 1.1.1.1 %%01OSPF/3/NBR_DOWN: Dead timer expired for neighbor 3.3.3.3. State changed to DOWN
[00:00:42] SIMULATOR %%CHAOS/5/RECOVERY: Maintenance finished. Powering ON router 3.3.3.3
```

### Anatomia de uma linha de log

```text
[00:01:28] 6.6.6.6 %%01OSPF/5/NHB_CHG: Neighbor 7.7.7.7 state changed to FULL
   │          │      │    │  │    │                     │
   │          │      │    │  │    │                     └──> Mensagem do Evento
   │          │      │    │  │    └──> Tag do Módulo
   │          │      │    │  └───────> Severidade (1 a 6)
   │          │      │    └──────────> Protocolo (OSPF ou CHAOS)
   │          │      └───────────────> Versão do Log
   │          └──────────────────────> Router ID de Origem
   └─────────────────────────────────> Timestamp Relativo (HH:MM:SS)
```

### Matriz de eventos

| Código | Severidade | Descrição |
|---|---|---|
| `CHAOS/1/KILL` | Alert | Roteador ativo derrubado pela rotina de caos |
| `CHAOS/5/RECOVERY` | Notice | Roteador ressuscitado após cooldown de 30s |
| `OSPF/2/SHUTDOWN` | Critical | Processo OSPF encerrado; threads e LSDB limpos |
| `OSPF/3/NBR_DOWN` | Error | Dead Timer (5s) estourou; vizinho vai para DOWN |
| `OSPF/4/LSDB_UPD` | Warning | LSU recebido com versão mais nova da topologia |
| `OSPF/5/NHB_CHG` | Notice | Handshake bidirecional concluído; vizinho vai para FULL |
| `OSPF/6/NBR_CHG` | Info | Primeiro HELLO recebido; vizinho entra em INIT |
| `OSPF/6/LSU_TX` | Info | Flooding de LSU disparado para todos os vizinhos FULL |
| `OSPF/6/SPF_RUN` | Info | LSDB alterada; Dijkstra recalcula as rotas |

### Build local (opcional)

Se preferir compilar sem Docker, você precisa de GCC ≥ 12 (ou Clang ≥ 15), CMake ≥ 3.20, Ninja e Conan 2:

```bash
# Instala dependências via Conan
conan install . --output-folder=build --build=missing

# Configura com Ninja como backend
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -G Ninja

# Compila
cmake --build build

# Executa
./build/simulador_ospf data/standard.json
```

### Executar os testes (build local)

```bash
cd build

# Todos os testes via CTest
ctest --output-on-failure

# Suítes individuais
./tests/test_dijkstra
./tests/test_ciclo_vida
./tests/test_ospf_convergencia
./tests/test_ressureicao
./tests/test_threads
./tests/test_simulacao
```

---

## Testes

| Suíte | O que cobre |
|---|---|
| `test_dijkstra` | Topologia em diamante, rede particionada, nó sem links, atalho vs. rota cara, nó isolado |
| `test_ciclo_vida` | FilaMensagens (timeout, produtor/consumidor, prioridade POISON_PILL), processamento de LSU, controle de thread |
| `test_ospf_convergencia` | Convergência FULL entre dois vizinhos, propagação de LSU por intermediário, ausência de convergência sem bidirecionalidade, descarte de mensagem para destino inativo |
| `test_ressureicao` | Limpeza de tabela ao ressucitar, preservação de links locais, reconvergência após ressureição, drenagem de inbox |
| `test_threads` | Lifecycle de thread, múltiplos roteadores em paralelo, ressureição sem crash |
| `test_simulacao` | Carga de topologia via JSON, inicialização sem deadlock, entrega e descarte de mensagens |

---

## Trade-offs de design

**Memória compartilhada vs. rede real** — a entrega de mensagens é feita por ponteiro direto à inbox do destinatário em vez de sockets. Isso elimina latência de rede e serialização, tornando o comportamento mais determinístico e os testes mais rápidos — mas remove a simulação de perdas de pacote, reordenação e MTU.

**Sem número de sequência em LSU** — o OSPF real usa sequence numbers para evitar loops de flooding e aceitar apenas LSAs mais recentes. Aqui a detecção de duplicata é feita por comparação de payload (`operator==` em `vector<Link>`). Funciona para a simulação, mas não escala para LSAs com campos mutáveis como timestamps.

**Dead timer simples** — o intervalo fixo de 5s facilita os testes, mas no OSPF real o dead interval é negociado via HELLO e pode variar por interface.

**`is_ativo()` sem mutex** — a flag `ativo` é lida por múltiplas threads sem lock explícito. Em teoria é uma corrida de dados; na prática, a semântica de `bool` atômico em x86 evita o problema. A versão correta usaria `std::atomic<bool>`.

**`rotina_caos` com vetores não protegidos** — `vetor_ativos` e `vetor_mortos` são reconstruídos a cada ciclo dentro da thread de caos, sem acesso concorrente real — mas o acesso ao mapa `rede` durante iteração poderia ser problemático em cenários de modificação dinâmica de topologia.

---

## Estrutura do projeto

```
simulador-ospf/
├── include/          # Headers públicos
│   ├── links.hpp     # Struct Link (aresta do grafo)
│   ├── mensagens.hpp # Tipos de mensagem e estados de vizinho
│   ├── roteador.hpp  # Roteador + FilaMensagens
│   └── simulador.hpp # Orquestrador da simulação
├── src/              # Implementações
│   ├── main.cpp      # Ponto de entrada + handler de SIGINT
│   ├── roteador.cpp  # Protocolo OSPF + Dijkstra
│   ├── simulador.cpp # Carga de topologia + injeção de caos
│   └── logger.cpp    # Log thread-safe
├── tests/            # Suítes GoogleTest
├── data/             # Topologias JSON
└── docs/             # Documentação interna e devlog
```
