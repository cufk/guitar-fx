# Stagebox – ikon och logotyp

Märket: tre fadrar (förslag D). Färger från appen:

| Färg | Hex | Används till |
|---|---|---|
| Beige | `#E8E5DE` | Bakgrund (ljus) / märke (mörk) |
| Spår ljus | `#CFCBC3` | Faderspår på ljus botten |
| Svart | `#111111` | Märke och text (ljus) / bakgrund (mörk) |
| Spår mörk | `#3A3936` | Faderspår på mörk botten |

Typsnitt i ordmärket: Inter SemiBold, spärrning −3,5 %, konverterat till konturer
(SVG-filerna kräver inte att typsnittet finns installerat). Inter är fritt under
SIL Open Font License.

## Filer

```
app-icon/
  macos/icon-macos-{light,dark}.svg + PNG 16–1024   Apples rutnät: rundad ruta 824 px i 1024, transparent kant
  ios/icon-ios-{light,dark}.svg + -1024.png        Fyrkantig utan transparens (iOS rundar själv)
logo/
  stagebox-logo-{light,dark}.svg/.png               Logotyp utan bakgrund (light = för ljus botten)
  stagebox-logo-{light,dark}-bakgrund.svg/.png      Med bakgrundsfärg
  mark-{light,dark}.svg/.png                        Bara märket
```

`light` är huvudversionen. `dark` är alternativet med beige märke på svart.
I logotypen är fadrarna något kraftigare än i appikonen, så att de väger
jämnt mot texten.

## Koppla in i bygget

1. Kopiera `app-icon/` till `app/Resources/` i projektet.
2. Ändra i `app/CMakeLists.txt`:

```cmake
if(GFX_IOS)
    set(GFX_ICON ${CMAKE_CURRENT_SOURCE_DIR}/Resources/app-icon/ios/icon-ios-light-1024.png)
else()
    set(GFX_ICON ${CMAKE_CURRENT_SOURCE_DIR}/Resources/app-icon/macos/icon-macos-light-1024.png)
endif()

juce_add_plugin(Stagebox                       # var: GuitarFX
    COMPANY_NAME             "Flatmate"
    BUNDLE_ID                "se.flatmate.stagebox"
    PRODUCT_NAME             "Stagebox"
    ICON_BIG                 ${GFX_ICON}
    # … övriga rader oförändrade
)
```

Byter du målnamnet från `GuitarFX` till `Stagebox` behöver även
`target_sources`, `target_compile_definitions` och `target_link_libraries` i
samma fil använda `Stagebox`. Kör en ny `cmake -B build -G Xcode` efteråt.

## I det här projektet

- Ikonerna ligger i `app/Resources/app-icon/` och är inkopplade via `ICON_BIG` i
  `app/CMakeLists.txt` (ljus variant; macOS respektive iOS). Målnamn, företag och bundle-ID
  var redan `Stagebox` / `Flatmate` / `se.flatmate.stagebox`.
- Logotypen i appens sidhuvud: `logo/app/stagebox-logo-{light,dark}.svg` – samma som
  `logo/stagebox-logo-*.svg` men med märkets transform förberäknad, eftersom JUCE:s
  SVG-läsare inte ritar den kedjade transformen rätt. Byts originalet: generera om varianten.
- De mörka appikonerna används inte ännu (macOS mörka/tonade ikoner kräver ett Icon
  Composer-paket som JUCE inte bygger).
