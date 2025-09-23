
# Processamento de Imagens com Histograma (SDL3)

Nosso projeto em C utiliza a biblioteca **SDL3** (com `SDL_image` e `SDL_ttf`) para demonstrar conceitos de **processamento digital de imagens**.  
O programa abre duas janelas principais:  

- **Imagem**: exibe a figura carregada em tons de cinza, com opção de aplicar equalização de histograma.  
- **Histograma**: mostra a distribuição das intensidades de cinza, estatísticas da imagem (média e desvio padrão) e um botão para alternar entre a versão **original** e a **equalizada**.  

---

## O que o código faz

- **Carregamento e pré-processamento**: abre a imagem escolhida, converte automaticamente para tons de cinza (se necessário) e ajusta o tamanho para caber na janela.  
- **Cálculo do histograma**: gera o histograma em tempo real com 256 níveis de intensidade.  
- **Equalização de histograma**: melhora o contraste redistribuindo os níveis de cinza da imagem.  
- **Exibição em janelas independentes**: a janela da imagem e a do histograma são abertas lado a lado, com interatividade.  
- **Botão interativo**: permite alternar rapidamente entre a versão original e a equalizada.  
- **Métricas automáticas**: calcula e exibe média e desvio padrão, com rótulos qualitativos (“Escura”, “Média”, “Clara” / “Baixa”, “Média”, “Alta”).  
- **Exportação**: salva a imagem processada em **PNG** ao pressionar **S**.  

---

## Como Compilar e Executar

1. Compile o projeto com `gcc` (ou equivalente), incluindo as bibliotecas **SDL3**, **SDL3_image** e **SDL3_ttf**
2. Execute o programa passando uma imagem como parâmetro:  
   ```bash
   ./histograma pavao.jpg
   ```

---

## Autores e Contribuições

- **Gustavo Ibara (10389067)**
  - Documentação  
  - Consulta teórica  
  - Carregamento da imagem  
  - Salvamento da imagem de saída  
  - Code review  

- **Nicholas dos Santos Leal (10409210)**
  - Documentação  
  - Criação do histograma  
  - Implementação das escalas de cinza  
  - Configuração do botão  
  - Refinamento geral do código  

- **Patrick Alves Gonçalves (10409363)**
  - Documentação  
  - Consulta teórica  
  - Estruturação das janelas SDL  
  - Implementação da equalização de histograma  
  - Testes e ajustes finais  

---
