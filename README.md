# Projeto-SO
Simulador de corridas

## Checklist
### Simulador de corrida
- [X] Arranque do servidor, leitura do ficheiro de configurações, validação dos dados e aplicação das configurações lidas (**META 1**)
- [X] Criação	do processo	Gestor de	Corrida	e	Gestor de	Avarias (**META 1**)
- [X] Criação	da memória partilhada (**META 1**)
- [X] Criação	do named	pipe
- [ ] Escrever a informação	estatística	no ecrã como resposta	ao sinal SIGTSTP
- [ ] Captura	o	sinal	SIGINT, termina	a	corrida e	liberta	os recursos

### Gestor de Corrida
- [X] Criação	dos	processos	Gestores de Equipa (**META 1**)
- [ ] Criação dos unnamed pipes
- [ ] Ler e validar comandos lidos do named pipe
- [ ] Começar e terminar uma corrida
- [ ] Tratar SIGUSR1 para interromper a corrida

### Threads Carro
- [ ] Atualizar SHM com as suas informações
- [ ] Gerir os vários estados de cada carro (corrida, segurança, box, desistência e terminado)
- [ ] Lerem as avarias da MSQ e responderem adequadamente
- [ ] Notificar o Gestor de Corrida através dos unnamed pipes

### Gestor de Equipa
- [X] Criar threads carro (**META 1 - preliminar**)
- [ ] Gerir box
- [ ] Gerir abastecimento dos carros

### Gestor de Avarias
- [ ] Gerar avarias para os vários carros, baseado na fiabilidade de cada um

### Ficheiro de log
- [X] Envio sincronizado do output para o ficheiro de log e ecrã (**META 1**)

### Geral
- [X] Criar um makefile
- [X] Diagrama com a arquitetura e mecanismos de sincronização (**META 1 - preliminar**)
- [ ] Suporte de concorrência no tratamento de pedidos
- [ ] Deteção e tratamento de erros
- [X] Sincronização com mecanismos adequados (semáforos, mutexes ou variáveis de condição) (**META 1 - preliminar**)
- [ ] Prevenção de interrupções indesejadas por sinais e fornecer a resposta adequada aos vários sinais
- [ ] Após receção de SIGINT, terminação controlada de todos os processos e threads, e libertação de todos os recursos
