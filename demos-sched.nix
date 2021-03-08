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
in stdenv.mkDerivation ({
  name = "demos-sched";
  src = builtins.fetchGit { url = ./.; };
  nativeBuildInputs = [ meson ninja perl pkg-config ];
  buildInputs = [ libev-patched libyamlcpp ];
} // extraAttrs)
