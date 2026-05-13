let
  rev = "c7f47036d3df2add644c46d712d14262b7d86c0c";
  sha256 = "1aclyh8aysw0d8gb1k9hh7mcklkqfvvv4f86l9zcipi4r0s9fal3";
in
import (builtins.fetchTarball {
  url = "https://github.com/NixOS/nixpkgs/archive/${rev}.tar.gz";
  inherit sha256;
}) {}
