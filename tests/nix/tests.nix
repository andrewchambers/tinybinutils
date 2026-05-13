{ pkgs ? import ./nixpkgs.nix }:

let
  testenv = import ./testenv.nix { inherit pkgs; };
in
rec {
  inherit testenv;

  tinybinutils = import ./tinybinutils.nix { inherit pkgs testenv; };
  cproc = import ./cproc.nix { inherit pkgs testenv; };
  sbase = import ./sbase.nix { inherit pkgs testenv; };

  all = pkgs.linkFarm "tinybinutils-test-suite" [
    {
      name = "tinybinutils";
      path = tinybinutils;
    }
    {
      name = "cproc";
      path = cproc;
    }
    {
      name = "sbase";
      path = sbase;
    }
  ];
}
