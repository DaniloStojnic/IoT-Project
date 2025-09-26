# Bežični sistem za nadzor tunela  

## Pregled

Ovaj projekat implementira bežični sistem za nadzor tunela.  

## Uređaji za simulaciju sistema

1. **Raspberry Pi** : koristi se za simuliranje **mikrokontrolera**
2. **Računar**: Koristi se za simuliranje **korisničke aplikacije**, **senzora** (co senzor i temperaturni senzor) i **aktuatora** (ventilator i sirena).  

## Pokretanje projekta
### Korak 1: Kompajliranje projekta

1. **Računar**
   
   U Makefile-u postavite standard jezika std=c++17.
   ```bash
   make clean  # Opciono, ako želite početi od nule
   make all
   ```

2. **Raspberry PI**

   U Makefile-u postavite standard jezika std=c++11.
   ```bash
   make clean  # Opciono, ako želite početi od nule
   make all
   ```

### Korak 2: Pokretanje mikrokontrolera

   U terminalu na Raspberry Pi uređaju izvršite:
   ```bash
   ./mikrokontroler
   ```

### Korak 3: Pokretanje korisničke aplikacije, aktuatora i senzora

   U terminalu na računaru izvršite:
   ```bash
   make run_all
   ```
   Na ovaj način pokrećete sve uređaje, osim mikrokontrolera, u različitim terminalima.

## Struktura projekta

- `mikrokontroler.cpp` - Kod mikrokontroler
- `senzor.cpp` - Kod senzora (temperaturni i co)
- `sirena.cpp` - Kod aktuatora (sirene)
- `ventilator.cpp` - Kod aktuatora (ventilatora)
- `aplication.cpp` - Kod korisničke aplikacije
- `tunnel_simulator.h` - Kod simulacije tunela

