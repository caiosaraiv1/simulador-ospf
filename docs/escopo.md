### 1. Alinhamento e Escopo (O "O quê?")
- **Qual é o problema real que estou tentando resolver?**
    O objetivo educacional e técnico é materializar a teoria do protocolo OSPF e algoritmos de grafos. O problema a ser resolvido é criar um motor de simulação onde roteadores virtuais consigam descobrir a topologia da rede sozinhos, trocar estados e convergir rotas utilizando o algoritmo de Dijkstra (SPF), servindo como um campo de treinamento avançado para programação concorrente em C++ e conceitos de infraestrutura.
- **Qual é o objetivo final e o valor dessa entrega?**
    Uma simulação de terminal rodando uma rede com cerca de 5 a 10 roteadores. O valor está em observar o cluster se formando e se auto-curando ao vivo, gerando logs precisos de eventos e recálculos de rota.
- **O que está FORA do escopo?**
    - **Múltiplas Áreas:** Será estritamente Single-Area (apenas Area 0 / Backbone).
    - **Redes Broadcast / Multi-acesso:** Não haverá eleição de DR/BDR. Apenas links Point-to-Point.
    - **LSAs complexos:** Não implementaremos LSAs do Type 2 ao 7. O protocolo viverá apenas da troca de Type 1 (Router LSA).

### 2. Entradas, Saídas e Regras de Negócio (O "Fluxo")
- **Quais são os dados de entrada (inputs)?**
    O estado inicial do mundo será carregado através de um arquivo `topologia.json`. Esse arquivo utiliza uma estrutura baseada em Lista de Adjacências (cada roteador detalha seus próprios links vizinhos e custos), o que facilita o _parse_ via biblioteca (como a `nlohmann/json`) e imita a essência de um Router LSA logo na largada.
- **Qual é o resultado esperado (outputs)?**
    Logs detalhados cuspindo na saída padrão (terminal), no mesmo estilo da saída de um comando de `debug` em equipamentos de rede reais (ex: VRP/Comware). Devemos ver cada transição de estado das adjacências e notificações sempre que a tabela de roteamento for recalculada.
- **Quais são as regras de negócio que governam essa transformação?**
    - **Adjacência:** Roteadores começam ignorantes (estado _Down_), trocam pacotes _Hello_ e evoluem os estados até _Full_.
    - **Compartilhamento:** Apenas vizinhos em estado _Full_ podem trocar seus LSAs.
    - **Armazenamento:** A base de dados de links (LSDB) é a fonte global da verdade, estruturada em memória como um `std::unordered_map<std::string, std::vector<Link>>` (uma tabela hash de adjacências globais).
    - **Convergência:** Qualquer alteração na LSDB obriga o roteador a engatilhar o algoritmo de Dijkstra imediatamente.

### 3. Arquitetura e Restrições (O "Onde" e "Como")
- **Onde esse código vai rodar?**
    Aplicação local, rodando via terminal, utilizando C++ moderno com suporte robusto a _multithreading_ (via `std::thread` ou paralelismo estruturado).
- **Quais componentes/sistemas serão afetados por essa mudança?**
    Cada roteador é o seu próprio universo fechado. O design segue o **Actor Model**: o estado interno (LSDB) não é compartilhado em memória global para evitar o inferno dos `std::mutex` espalhados e _lock contention_.
- **Existe algum padrão de design ou arquitetura já estabelecido no projeto que eu deva seguir?**
    - **Concorrência Baseada em Mensagens:** O protocolo opera de forma assíncrona. Cada roteador vive em sua própria _thread_ e possui uma "caixa de entrada" (uma estrutura _Double-Ended Queue_ - `std::deque` - protegida por um mutex local). A única forma de comunicação é injetar mensagens na fila do vizinho e deixar ele processar no tempo dele.

### 4. Caminhos Críticos e Casos de Erro (O "E se?")
- **Gargalos de Processamento (A Fila Cheia):**
    Em um cenário de instabilidade massiva na topologia, as filas de mensagens (`std::deque`) de alguns roteadores podem ser inundadas simultaneamente por dezenas de atualizações de rotas (LSUs). O comportamento esperado é que a _thread_ consiga absorver esse pico de processamento sem gerar contenção de memória, sem descartar pacotes vitais e sem bloquear a _thread_ vizinha que está tentando enviar a mensagem.

- **Partição de Rede (_Split-Brain_):**
    A injeção de caos pode derrubar roteadores estratégicos no núcleo da topologia, dividindo a rede em duas "ilhas" isoladas que não se comunicam mais. O motor que roda o algoritmo de Dijkstra precisa lidar com esse particionamento de forma fluida. Ele deve processar os novos grafos desconexos sem tentar acessar nós inalcançáveis na memória e sem entrar em _loops_ infinitos durante o recálculo.

- **Morte Súbita (_Chaos Engineering_):**
    A resiliência será testada derrubando roteadores aleatoriamente no meio da simulação. Para não causar um _Segmentation Fault_ interrompendo uma _thread_ à força de fora para dentro, o sistema aplicará uma **Poison Pill** (Mensagem de Controle _Out-of-Band_). Essa mensagem letal é injetada diretamente no topo da fila do roteador-alvo (`push_front`). Ao ler a fila, a _thread_ engole a pílula primeiro, limpa a própria memória, quebra o laço de eventos e morre de forma limpa.
- **A Recuperação e Cura Pró-Ativa:**
    Quando a morte súbita acontece, a _thread_ do roteador deixa de enviar pacotes _Hello_. O mecanismo de tratamento de erro primário passa para os vizinhos: as _threads_ adjacentes percebem o estouro do _Dead Interval_, agem proativamente alterando o estado do link para _Down_, geram uma nova versão do seu LSA e inundam a rede, acionando uma reação em cadeia controlada para que todas as outras _threads_ recalculem a tabela de roteamento e curando o cluster.
