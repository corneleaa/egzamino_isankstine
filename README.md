# egzamino_isankstine

## Projekto aprašymas
Šis projektas realizuoja teksto analizės užduotį, naudojant **C++** kalbą, **std::string** ir **asociatyvius STL konteinerius**. Programa automatiškai parsisiunčia tekstą iš lietuviškos Vikipedijos, jį apdoroja ir sugeneruoja kelias ataskaitas:

1. Suskaičiuoja, kiek kartų pasikartoja kiekvienas skirtingas žodis tekste.
2. Sudaro **cross-reference** tipo lentelę, nurodančią, kuriose teksto eilutėse kiekvienas daugiau nei vieną kartą pasikartojantis žodis buvo paminėtas.
3. Suranda visus tekste esančius **URL adresus** (tiek pilnus, tiek sutrumpintus).

Projektas skirtas pademonstruoti asociatyvių konteinerių (`std::unordered_map`, `std::set`) pranašumus ir praktinį `std::string` naudojimą.

---

## Naudojamos technologijos
- C++17
- CMake (≥ 3.16)
- libcurl (HTTP užklausoms)
- STL:
  - `std::string`
  - `std::unordered_map`
  - `std::set`
  - `std::vector`
  - `std::regex`

---

## Duomenų šaltinis
Programa automatiškai parsisiunčia tekstą iš lietuviškos Vikipedijos naudodama MediaWiki API.
https://lt.wikipedia.org/wiki/Feminizmas


Naudojamas `explaintext=1`, todėl tekstas grąžinamas be HTML žymėjimo ir be aktyvių hipersaitų.

---

## Programos veikimo logika

### Žodžių išskyrimas
- Tekstas skaidomas į žodžius, atmetant skyrybos ženklus.
- Žodžiai normalizuojami (ASCII raidės konvertuojamos į mažąsias).
- Ne ASCII simboliai laikomi žodžio dalimi.

### Žodžių pasikartojimų skaičiavimas
- Naudojamas `std::unordered_map<std::string, int>`, kuriame:
- raktas – žodis,
- reikšmė – pasikartojimų skaičius.
- Į ataskaitas patenka tik žodžiai, pasikartojantys daugiau nei vieną kartą.

### Cross-reference lentelė
- Naudojamas `std::unordered_map<std::string, std::set<int>>`.
- Kiekvienam žodžiui kaupiami unikalūs eilučių numeriai.
- `std::set` užtikrina, kad eilutės būtų:
- be pasikartojimų,
- automatiškai surūšiuotos.

### URL paieška
- Naudojamos `std::regex` išraiškos:
- pilniems URL (`https://...`, `http://...`),
- sutrumpintiems domenams (`www.vu.lt`, `vu.lt`, `saitas.xyz`).
- URL deduplikuojami ir surūšiuojami prieš išvedimą.

---

## Išvesties failai
Programa sugeneruoja šiuos failus:

- `downloaded_text.txt` – parsisiųstas ir apdorojamas tekstas.
- `words_report.txt` – žodžiai, pasikartoję daugiau nei vieną kartą, ir jų skaičius.
- `crossref_report.txt` – cross-reference lentelė su eilučių numeriais.
- `urls_report.txt` – visi surasti URL adresai.

URL adresai papildomai išvedami ir į konsolę.

---

