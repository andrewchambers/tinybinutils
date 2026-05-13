{ pkgs ? import ./nixpkgs.nix
, testenv ? import ./testenv.nix { inherit pkgs; }
}:

let
  inherit (testenv) mkCproc qbeHost tinybinutilsHost cprocHost;
in
mkCproc {
  pname = "tinybinutils-test-cproc-bootstrap";
  cc = "${cprocHost}/bin/cproc";
  qbe = qbeHost;
  tinybinutils = tinybinutilsHost;
  doBootstrap = true;
}
