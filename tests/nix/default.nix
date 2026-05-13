{ pkgs ? import ./nixpkgs.nix }:

(import ./tests.nix { inherit pkgs; }).all
