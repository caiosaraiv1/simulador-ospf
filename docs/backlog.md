### Fase 1: Estruturas Base e Input

- [X] Configurar o projeto C++ (CMake ou Makefile) e importar a biblioteca `nlohmann/json`.

- [X] Criar o arquivo `topologia.json` de teste com 5 roteadores e seus links iniciais.

- [X] Criar as `structs` ou `classes` fundamentais de representação de rede (ex: `RouterID`, `Link`, `LSA`).

- [X] Implementar a função de inicialização que lê o JSON e instancia os objetos em memória (sem depender de `sscanf` ou parse manual).


### Fase 2: O Coração do Roteamento (Matemática e LSDB)

- [X] Implementar a estrutura da LSDB usando `std::unordered_map<std::string, std::vector<Link>>`.

- [ ] Implementar o algoritmo de Dijkstra (SPF) capaz de navegar por essa LSDB e calcular as rotas mais curtas.

- [ ] Criar um teste unitário simples: popular a LSDB manualmente e garantir que o Dijkstra retorna o caminho e o custo corretos.


### Fase 3: Concorrência e Modelo de Atores (O Motor Paralelo)

- [ ] Criar a classe principal `Roteador` e fazer seu método de ciclo de vida rodar em uma `std::thread` independente.

- [ ] Implementar a "Caixa de Entrada": um `std::deque` encapsulado com `std::mutex` (ou `std::unique_lock`) para ser 100% _thread-safe_.

- [ ] Definir o enumerador/struct das mensagens que transitarão nas filas (`HELLO`, `LSU_UPDATE`, `POISON_PILL`).

- [ ] Fazer as threads se enxergarem e conseguirem dar `push_back` na fila dos vizinhos conectados.


### Fase 4: Dinâmica OSPF e Máquina de Estados

- [ ] Implementar a lógica de tempo: disparo periódico de pacotes `HELLO` para os vizinhos mapeados.

- [ ] Implementar o controle de estado da vizinhança (transição de _Down_ para _Init_ e para _Full_).

- [ ] Fazer com que, ao atingir o estado _Full_, o roteador empacote seus links em um `LSU` e envie aos vizinhos (inundação/flood).

- [ ] Ligar a recepção de um `LSU` novo à atualização da LSDB e ao disparo automático do Dijkstra.

- [ ] Formatar o log de saída padrão (terminal) para imprimir as transições e cálculos no estilo CLI de equipamentos de rede.


### Fase 5: Chaos Engineering e Tolerância a Falhas

- [ ] Criar uma rotina geradora de caos (uma thread separada ou menu interativo) que escolhe um roteador aleatório para derrubar.

- [ ] Implementar a injeção da `POISON_PILL` usando `push_front` na fila do roteador escolhido.

- [ ] Implementar o comportamento da _Poison Pill_: a thread do roteador deve limpar sua memória, dar _break_ no loop e morrer graciosamente.

- [ ] Adicionar o timer de _Dead Interval_ nos roteadores vivos: se ficarem X segundos sem receber _Hello_ de um vizinho, marcam o link como _Down_.

- [ ] Garantir que o link _Down_ gere um novo LSA e force a rede inteira a convergir e recalcular as rotas proativamente.
