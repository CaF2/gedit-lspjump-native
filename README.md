# Lspjump-native

A rewrite of my own LspJump plugin but this time in C.

Does not currently support everything from the python version, but we will get there eventually.

# Installation

Just follow these steps:

````
cd ~/.local/share/gedit/plugins
git clone https://github.com/CaF2/gedit-lspjump-native.git
cd gedit-lspjump-native
make
````

Then go into gedit -> settings -> plugins and enable this plugin

If it does not work, check that you have the gedit-devel package installed.
