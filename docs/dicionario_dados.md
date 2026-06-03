- **Struct `Link`** (A base da representação do grafo):
    - Atributos:
        - `std::string destino_id` (O Router ID do vizinho)
        - `int custo` (O peso da métrica OSPF)
        - `std::shared_ptr<FilaMensagens> inbox_vizinho` (Ponteiro inteligente para a caixa de entrada exclusiva do vizinho).

- **Enum `TipoMensagem`** e **Struct `Mensagem`** (O protocolo de comunicação):
    - Atributos:
        - `enum TipoMensagem { HELLO, LSU, POISON_PILL }`
        - `TipoMensagem tipo`
        - `std::string remetente_id` (Quem enviou a mensagem)
        - `std::vector<Link> payload` (Usado para carregar os links em mensagens LSU, vazio nos outros casos)

- **Classe `FilaMensagens`** (O encapsulamento thread-safe):
    - Atributos:
        - `std::deque<Mensagem> fila` (Fila de duas pontas para permitir "furo de fila")
        - `std::mutex mtx` (Para evitar Race Conditions)
        - `std::condition_variable cv` (Para acordar a thread do roteador apenas quando chegar mensagem, otimizando CPU)

    - Métodos:
        - `push_back(Mensagem msg)`: Insere tráfego normal (Hello, LSU) no final.
        - `push_front(Mensagem msg)`: Insere a Poison Pill direto no topo (OOB - Out-of-Band).
        - `pop()`: Retira e retorna a primeira mensagem da fila.

- **Classe `Roteador`** (O Ator autônomo):
    - Atributos:
        - `std::string router_id` (Ex: "1.1.1.1")
        - `std::unordered_map<std::string, std::vector<Link>> lsdb` (A Link State Database local)
        - `std::unordered_map<std::string, auto> timers_vizinhos` (Guarda o timestamp do último Hello recebido de cada vizinho para calcular o _Dead Interval_)
        - `FilaMensagens inbox` (A caixa de entrada exclusiva dele)
        - `std::thread thread_trabalho` (A thread onde o ciclo de vida vai rodar)
        - `bool ativo` (Flag de controle de vida)

    - Métodos:
        - `iniciar()`: Dá o disparo na `thread_trabalho` e entra no loop infinito.
        - `processar_mensagem(Mensagem msg)`: Pega a mensagem do `inbox` e decide o que fazer via `switch(msg.tipo)`.
        - `enviar_hello()`: Dispara pacotes HELLO periódicos para os links mapeados.
        - `executar_dijkstra()`: O motor SPF que roda sempre que a LSDB sofre alteração.
        - `suicidio()`: Executado ao ler a POISON_PILL; limpa a memória, imprime o log de morte e encerra a thread limpidamente.

- **Classe `Simulador`** (O motor central e injetor de caos):
    - Atributos:
        - `std::unordered_map<std::string, std::shared_ptr<Roteador>> rede` (A lista de todos os roteadores instanciados)
    - Métodos:
        - `carregar_topologia(std::string caminho_json)`: Usa o `nlohmann/json` para ler o arquivo e instanciar os Roteadores e seus links iniciais.
        - `iniciar_simulacao()`: Chama o `iniciar()` de cada roteador.
        - `injetar_caos(std::string router_id)`: Encontra o roteador alvo e chama o `inbox.push_front()` enviando uma mensagem do tipo `POISON_PILL`.
