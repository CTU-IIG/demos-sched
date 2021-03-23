{ stdenv, meson, ninja, perl, pkg-config, libev, libyamlcpp, extraAttrs ? {} }:
let
  libev-patched = libev.overrideAttrs (attrs: rec {
    pname = attrs.pname or "libev";
    version = "4.31";
    src = builtins.fetchurl {
      url = "http://dist.schmorp.de/libev/Attic/${pname}-${version}.tar.gz";
      sha256 = "0nkfqv69wfyy2bpga4d53iqydycpik8jp8x6q70353hia8mmv1gd";
    };
    patches = attrs.patches or [ ] ++ [
      ./subprojects/libev-Add-experimental-support-for-EPOLLPRI-to-the-epoll-b.patch
    ];
  });
  # Stable Nix cannot work with submodules so we construct the source
  # tree with submodules "manually".
  srcWithSubmodules = stdenv.mkDerivation {
    name = "demos-src-with-submodules";
    srcMain = builtins.fetchGit { url = ./.; };
    srcEv   = builtins.fetchGit { url = ./subprojects/libev; };
    srcYaml = builtins.fetchGit { url = ./subprojects/yaml-cpp; };
    buildCommand = ''
      cp -a $srcMain $out
      chmod +w $out/subprojects
      copySub() { rm -rf $2 && cp -a $1 $2; }
      copySub $srcEv $out/subprojects/libev
      copySub $srcYaml $out/subprojects/yaml-cpp
    '';
  };
in stdenv.mkDerivation ({
  name = "demos-sched";
  src = srcWithSubmodules;
  nativeBuildInputs = [ meson ninja perl pkg-config ];
  buildInputs = [ libev-patched libyamlcpp ];
} // extraAttrs)
