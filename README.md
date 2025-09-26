# Bežični sistem za nadzor tunela  

## Pregled

Ovaj projekat implementira bežični sistem za nadzor tunela.  

## Uređaji za simulaciju sistema

1. **Raspberry Pi** : koristi se za simuliranje **mikrokontrolera**
2. **Računar**: Koristi se za simuliranje **korisničke aplikacije**, **senzora** (co senzor i temperaturni senzor) i **aktuatora** (ventilator i sirena).  

## Pokretanje projekta
### Korak 1: Kompajliranje projekta

1. **Računar**
   
   U Makefile-u postavite standard jezika std=c++11.
   ```bash
   make clean  # Opciono, ako želite početi od nule
   make all
   ```

2. **Raspberry PI**

   U Makefile-u postavite standard jezika std=c++17.
   ```bash
   make clean  # Opciono, ako želite početi od nule
   make all
   ```

### Korak 2: Pokretanje projekta

1. **Računar**

   U terminalu izvršite sljedeću naredbu:
   ```bash
   ./mikrokontroler
   ```
   Na ovaj način pokrećete mikrokontroler.

2. **Raspberry PI**

   U terminalu izvršite sljedeću naredbu:
   ```bash
   make run_all
   ```
   Na ovaj način pokrećete korisničku aplikaciju, akutatore (ventilator i sirenu) i senzore, u različitim terminalima.

## Struktura projekta

- `mikrokontroler.cpp` - Kod mikrokontroler
- `senzor.cpp` - Kod senzora (temperaturni i co)
- `sirena.cpp` - Kod aktuatora (sirene)
- `ventilator.cpp` - Kod aktuatora (ventilatora)
- `aplication.cpp` - Kod korisničke aplikacije
- `tunnel_simulator.h` - Kod simulacije tunela

