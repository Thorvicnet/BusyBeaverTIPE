# Busy Beaver TIPE

## Compilation

```sh
make
```

## Fichiers

- `main.c` : parcourt les machines et affiche les statistiques.
- `beaver.c` : exécute une machine de Turing.
- `encoding.c` : convertit un identifiant en table de transition.
- `tape.c` : gère le ruban.
- `drift.c` : détecte les cas cycler exacte et cycler translaté.
- `formula.c` : possède les 4 compteurs binaires et l'invariant régulier.
- `bouncer.c` : vérifie les certificats bouncer pompés.
