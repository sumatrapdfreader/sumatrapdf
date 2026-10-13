/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

/*
 * \file  bootnumgen3.c
 * <pre>
 *
 *   Function for generating prog/recog/digits/bootnum3.pa from an
 *   encoded, gzipped and serialized string.
 *
 *   This was generated using the stringcode utility, slightly edited,
 *   and then merged into a single file.
 *
 *   The code and encoded strings were made using the stringcode utility:
 *
 *       L_STRCODE  *strc;
 *       strc = strcodeCreate(103);   // arbitrary integer
 *       strcodeGenerate(strc, "recog/digits/bootnum3.pa", "PIXA");
 *       strcodeFinalize(&strc, ".");
 *
 *   The two output files, autogen.103.c and autogen.103.h, were
 *   then slightly edited and merged into this file.
 *
 *   Call this way:
 *       PIXA  *pixa = l_bootnum_gen3();   (C)
 *       Pixa  *pixa = l_bootnum_gen3();   (C++)
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

/*---------------------------------------------------------------------*/
/*                         Serialized string                           */
/*---------------------------------------------------------------------*/
static const char *l_strdata_0 =
    "eJy9nXk01P37/2cMM4NhZqxjnbGPsoxdhRn7WpGSiowlqSSyJmXGTgiplJSt0o42oYx9iUIq"
    "FWXQolRoG4X5jmXcy/id07v7fH5/uO/7zHHu8354vd7X83ldr+t1DZ/jjghPnMu2vcE79gTg"
    "tPnWhO722rYXt8cXF7gjAmeM09I15OMz2/P/+B2vPRHbglm/ReCb/fUtBPeVuIi9c5/oEAhq"
    "uH2L/82X7LjGWoBPkg8EAgnY2lisY/1bkvWjBGb9AxQi0yDF+hdvoM2mYBBIQ2H2Bxxxlhg2"
    "+2GIpWuI+Z7du7cFhIAMC/B6OawPt9lamK6Hn9jwoIOXQkDwDGw0ingk4Ha2/3LQdYOUAS5Z"
    "YxGv02fz5aJWDCfS1qpIm5orBfgM1tlJ5/DKJ9sEFPIZ1GJDn9p0P7//NT7Pr1focR9flrM0"
    "xhETaaEun/ALNuT3aFfACeQE2mql12jY+9lHtLVcY3HVjBwzx6kFiBPD+lGY4zxX9mPVb3ES"
    "rn25e5H14Xo2Z5kAi9Oi67pbsQ/VRiK+Q15B7D4+lWRryUCdFLm2ojX8yZWr9pbnxWT87M5k"
    "IJ+JiLrDd9fImOwQVtjvRFz/Scfnq9SWB75mntGi0UboD8srdnaYenBgaQNePsU5rBpT8Xu/"
    "haXV+zXNiPWh1TyWN0OByQTVg6G4Lkq8IQ2uKEtJwuIGoxWS/EBoGL0BPJEEK7EwN26RQOD2"
    "S0hjMPnckFM8WWQj2hiD+AsM2p/gIeq8Z/VHDhAdQCBSiyCOsVGU3wLRNp3uS2B96DcPsu1B"
    "GZqGQ5g7BYmo6pj1Kl4q5ZVNczLiUuJJVFDa5la97hW8SloqsT6De+t7a/GhGzH7Vtyxrg2A"
    "JDHwDZbiLrwwr+BlLmJu6apNtqdulirH9Fu4KOtcu+3SM+0uftVD+UwUSO2RTf3+gmE7DlRd"
    "QKjii6h2NacEfwtVR//nKIr14eZ/bcVruLM+XA3S61MTjCQdaHysvUjV6LWVTHl6ao1JkamR"
    "f1/7aKrr0QI7gpJDcr3o6ybtiZcvLqS0r0oId+P9HhzZZFb1otFldOvlmUQTfpvlAWYdlfc4"
    "4PQAwUkvwpV7bJ/8LTjdktP7rrM+9JyH291ey4onKG7aUaOJVFE//hHaMTU6LXG3u+rGOuMD"
    "cQ2r4lG1BxBq/IdcjhQe9StREAxvMcVcQNYmCK1pFM3zeffePzHGw8aUf0Y7loKAjt5Yg23w"
    "HM0zctkRCXmMsx+wvKNE5WDUB8Qosci4Puzy3d9i1Ht1wlaJ9aH7PGMoa69SCHCezodn80O4"
    "rVv9PHX7LG7b0CUm0R8dxs00sIIhJZ7WWoxYTOqG8gy0rvbLtuVOJl33TgnZvikZ8tnfwMVU"
    "OHibP2nYqK4K1dGaftTexfywe0DFPuOA97fThTn4DP7XfPpWGZgzrA/d/r5BUeZO1qrdtfDU"
    "rK7BLm07q/xi0soJtKrfVs+8KJev/XyiHxL413T4kzbmFef0xL9AXqgF82grdsrbVKzu8oyC"
    "O/jaVScf1+TV1nmSn6tOH4Os2GlIuOIZ94wDzxAw3rzknVkWjfktPAOt43he1odb5vHc2+fe"
    "P27a4XWO2lxFpRihMyk35cvgCsXFJK1bSoeSNNd8rjuccgJXngnvsPZ5dB2e7MwnB2nt9Bh5"
    "UGyUnt427c17MuiKqV4FLNCneL+9lb4gnX9Id22eZG5oIgfdiv+10LEF3WmebvWDKgEaAQ7q"
    "MtfI0pBSKqr1Sx1ErzuX5OM9cfOrRTA8uFDaf1RtvfIpIW6pJ52jzcGVdvobf7XmSSieFNq9"
    "45sSWO/08Y93DT41YVfu9lDZbOeSuJFTvIG5lD8ImSueaPII/rUj9R7MRhWEFX17vKxMra2F"
    "uLBqpat1K9w4Ep8WbQl5pT7MUyN51IpOc7V1EKLf5vaj1lY5T6gku4S9tD2jpmz5HvrJnRZa"
    "v8tRda892vl+ztnb3+p7Zb/WqK7K8Rrl5+QD7k7mt2TgOxFlQO5k06L6zUvCnrM2tvbN8nB9"
    "hQe+qXVwVVX51CPyQoURj1vDmwqdGitsRb5/8hpUDFsJCwk+nK/so3LOnGEY3bDt4fMrsgGh"
    "cV2vbvEdsl+PYL7lOU7QjjyS3rKdEw6YRxFf3JEbKy/fAuRRNBY8ih+Z2UyjpSZIyDFAKCZk"
    "kInrs5ELYDSPJRakF1Cph8FSfmQyCGQMXqlSeFeCh/OJgZmRP4gQbDOyY0HEOsoE4gki5k4K"
    "fG3+oCKCpO/1VCF9Srp8XW+2BzFlzZHqcLmkd67JwvZ4i3qL9amecEJwr430xbiDz2KJH8Zz"
    "IivuWp7/cWHfdaPRY7s8JiS6ZTqMT7g4qIu7t05HT7y38nC55zVgnadffMSUExaYHcEsvltV"
    "N5pjAdmRhb3nnjYX7SGadplZaNfY9K4WP09hq3wH0SEYn0VQ5MjuyXdJsUebHLZxpbQi7c+l"
    "N5hoZyW/4N/Z1D9u2kCcGA5wjI7o2vI1/Nf3OM3q3OlBtUrQrtfq53a1ns/khANuR+b33rXN"
    "G00B2RGfv71YOBHuTh2R7ErqckPq1QZohmuFWBGhTlMVHnHcr6gmotF97ySmX8pK+HGmfLnN"
    "DNJGvIM3WuK9mosXTFW++VGG/MZS79q3WS3k2tNF/Xd7bLH4+3Cv/fg6VMCjnbbcazabHeXk"
    "BGZJ/iCAsC3J6r927Kxkd2HP5lejMUpFr8s3WXmi4u5YbJRL2bjpVqaBMux0r9qpznj7VORt"
    "EvLY3Up3ka/c6ClYaovnhcmbN+4aHKpYrdnzmLBhxBzkxgkEzIOIsH5k5oBipoP8AC2cHTux"
    "YY5D6pkwxyhQihQlhw6pJ6LEGLSxIlASMYrMADUgqQwtFBY6GIb3PehIo9FwNBBEgPSdCYGS"
    "dPhxbw6SakEKu4k58asOx3PSALMcQqwf7BxN3fJ8RUCOasOCKM9aDhyGh64jlp2qmHuhIPBK"
    "szJGPL8YN4D1Lmzc+FJLQ6vw3RpdzffybTsTDFdHfvTN7VAkta8Y4Kvgu3eg76HnoELGTpz4"
    "r80eHx/dDchn6Op+eqDbxgkGzG0IL4Kpj9yXBCTMC7mM8fy+g5DsVddpc9XwpmY5kVTLXXqH"
    "n+FeizvFYBV93HdIHXC6kT/mdjJji5mWCt4d5cHgaXtAiX0vqM9jFatsYJKyaUTx0+ESAnPz"
    "5A3VTxeQjBjdV64DLTkcdNrAbIfIIt1DvU5vQLJsw96E9VNUOqweloDkxWJkp/DkCEY9hT6A"
    "hSS8IXIhOpi4nGksxDZHIqn2JxXj88Q0/wTuOlGENPqznkGlE5kwUPxpzaK6VXJgThZgFmN2"
    "C8rO/V94w4OpgDTNcSEStpfxUkgi3F1o/lJu+/GVX2g0bbVGUalJJTc/3OnlGLhgdQg+L8ml"
    "BC2htb3Mwks9+afCzmr9F7vLn03diQwSHMRcSfHyHNwS2cenov9p86d0V04mYM4Cvbg+qtJS"
    "KoCka8HrurOj3mH8ulknf/ICfpNqkmWWAAZnF8i9gZVI+2O3Ce9Aa8E7vM407Es+BvvxGfX4"
    "TveKLeOrxdRN91v9jFCIEP/xrsblbu5y/Z7dh61WcUIBMx/Ci5GPLLcTDSjyWf4V+cDzkS9p"
    "NvKB64n4GUgdDxJzMCqCNoZOwnKNe+I/wbwO5ptY5ZuEjDFJXJ0OOOhF01oslpXk+BhdsN+k"
    "yMcJAsxY/EFsYAc9l4U0cnZ1SAgISSEl+/JYbilf7pjQofjVRmZtmvKyNSgbkY2NYZ3e5uHg"
    "IIlM/i4X+e+wVXTN/gPyF8s35K3gu2wryFhhwvM2X/rb8p7dz22GBMR09JPMJSR3c5IBcxXo"
    "xSXy6alU+y0ydgZptOho/cgkHgHEBFYSMxM7Zo0/CG88TGUQRLAwliDNrg18RgXZ+F0AEZ3P"
    "gMGfmo8zoSD1/dp8u+6+LOJ8fmBu4Q9iATtHdJ5/fsv22tl0A0IqMil3sfIUuWjiej/J8rnA"
    "RLuwuanRmhT5FzYKkXLpa07Re4N3nsFdDMTfRhX49364900qt3L9rUtSlNfXjN2kBK8GK/Br"
    "Vjno3Ggn+jdwYgHzDH+gsmwxWqjxrp4LBwieLnMRVR2uoowOkFPpE2u1QbTbOgtuZ9zZiI8y"
    "oUImfFINSkGuY2aVxIxVF3QS+k2XYwZEdvp9F/6I2JI0c8SiY/xE/LHOr1w2w1pvL4QORnBy"
    "AXMP/0GGrNkRoZlJpSO/sGQolTTFskL1WB5EIhNDGR+PhiI+ktVoNMFmhAUZXHraFwU9GYyK"
    "o74xYTjSQDfi6TCF3jUgy0Yt5zeoHeKcKMD8wn94c1YtoBjSOkFxCDICipTEDBN1CVOJ9MM2"
    "DDkRbMLgEKlPsHHaJgeb9FoKM0MtPQCpx6KuJnqDNnboOWJ763ZyPL4OMEMAX3z8mj2o36tD"
    "sFNZWfbjt3S20OANuAiUgCzUfL2LgjkEwnLvYpvQviIeL5ZxPiEwmUcvvtqDpJYYQH9gncU/"
    "MI3WSaPwzf1xrZuQmWOe+IA5m8yKStwzKodp0aNyszGpXgtU0qT1roW4bgfnYwNT8v/gTtYt"
    "FHjaF0qOzk7as29uhYZ3aYoS3xMbfYqGZTY5u757l1tUcfyAQ6+uhYKgkPGdVYaCr473qx63"
    "cDp/pRwdqmKnIttax4WZ4aHz3z2z8p3pWUk5TipgUv4fFoO92xkLux3yHTknFDDcglAImQg2"
    "fHTsQzZMwafWylKZveZBxtxMUBdTCrR+u268F++0NefjAxNw1GLcaXoZKAHIXjksxNM6lmUk"
    "iFh2SR0ct7cOzL4sVlYacpb70GW0QQuo9jw8JtH8sK6KVGuvgHpJFHJ9V8KpcV2rAT58Qerz"
    "KuZpB54ttZ4JERDnNSoiYzx3Qjl5gMv2Hy7HikVnVUYFc6FJM6YQ8+lEaAKTMIVKwULGPRWj"
    "pFNeFORgY5it5mXGXEwwPRI+86sGDOL9pB++99Y+zjMHHWCa/R9soc0/bSFpISGes4VFYxS6"
    "NRcK8jiKe7CZNCWddBgStAr1BaK135wLAonlgiCkQeW1voVDh8kI0KbTxul5ZZZJnCzAhPoP"
    "FoJdrXD8ewV+zrfjJahOQc2bPY/4xHaeTQoxxIs27ooyERaS6bosmBhhlq2a90Ztt/y1Txry"
    "fV+SW5xvftcpf7+1bIvbzFGD3poO8QM7dR1Wh6Y7czIBT/Gl55gsarpTAIn0om1XiKbQIb/A"
    "CbB2GEaQIEv2C2SJHWMAZP4FAiP4zrDi8hpd6EXYh9icrkCUn3JnH4z0hTiGJTM8o0GgNILK"
    "rV898NucIMAkWoz1ozwH8u6my+8dILDf+oXjkQPsWrSm0jYrcjaiESRiqtbEWqduS59gcNe6"
    "6vdRK4d2kBErpVSONsnznNOL3fY9c4WiiUuSxpWByvA2BccnwzGBydi2Izus8p72nXoROrMX"
    "EuWtvcp/aqqUA08XmITPxrFlc3gBUR0dgPbewkHz7vnDLQjNWiSbtfcw6tgyV9QhiitFW7Qp"
    "Ac5rEXz01UFz3oBnm7sqJo8/UAcfHV/9PN9G5C78W2HdOmJLapeAjVdkC9EN/clsO95ru6i2"
    "6kWPTwWmgX2S6K9WistrVoLP8WzZsKe6O5iTFZgZmF1K/Bxrd42B/W+xsjMwz7+XnUTi6Gf8"
    "zOrhylcHQMoavvIBTfhUsq0pzTxtc9S1feHW6umSjav40ttJ3avd35Rq+w5m8b7YfQ2bTd3g"
    "G3RY9tHWbw89Rov2U9HXqs/gSqRudinuFnsi3OGjNwFLeoPghARmHf5DZ8fGhcJGx8LZkIrx"
    "8Dt4+qBXjLq7WkA3OvIrLm03N9ooCrWXgRVLO5R35GibmacVOkS4kCfrPnf4VNKjgx/eHnc7"
    "aLzqecxnsYZVRQaVV37sLpd606cvX1L7o5oTDZh/EFtEq4hoTvotNLbd1Fr0D1OJBdRxJAQi"
    "BB1HgsajUQHF8tCTPyhU1sdUniSJAQYIFU0eAIOmEk3D5NzFsjgfGphrmH3o+fOEswd+EABZ"
    "ua2LWdisa4ijE1GlrXBVaYtArUOU+F3bslMzuuWH90mikfc+9Ru3663RX8Yfb6OcpFKgGwit"
    "kj5WijQX73f9lhM5jW4+HXa/p2WXwrOBre2mQ6ejP0W7DPJMrtYWL6hmqnASAvMRmMUQQr13"
    "7gugCOnFPv8vQLNCiEXXmeSkYm67Ah3DBC4/1bM0XGkm/pg9lGpqFFp1PTFMDqoRqb9S+0KZ"
    "VtbNwJU7ZuAahYNqQfvf2r98/Ob2qupnz1uJ7V3lzaJRBl6hVRdqECLKN96DNw3YoQ4Fojiz"
    "aV3gXQ7/LVAeeGDMx3qvEujZgjLGZocHuxzGybcMxqGVI7imajiP+PFE7ntTSScuXM7acFEx"
    "SeZ5dYmu/Ov7fcuP8lphcKZxA644t/X6hF2O0U5jWlX0rxtc8g51rDJY9vleOASriMmrT8Q6"
    "n/Ko0B+mreZkBd7xAJCVHSj3/KMlh3tWFFJpeAmB7TrXk63zHdCeMzzowhXPNGbS2hszVjc4"
    "uGGe8ucWBFudsdnSciclzQJdZTOIxXuJOmi1x2qP39q4ZiThJyphiIfotu+qoLOUDhfCUPSx"
    "RXOE9oYTtRPDMAP4M7Kc7onkDZzIwPyK+KI2GB9wC/0tZHYNyJ0dW6KnsBD4DLnUO4KGaw6k"
    "UJFJzzCSM2BWkPH18rOoh1H2wnb2Ut5PYkxI4xA89MMV0EhiybaZV6CmUcf21DgJpg4lyFwG"
    "ZU+gfjNrz4ZUvSXWE0G8Ajqd6+W3KnECAm+EWD7fMzAY6vJbgOxqkP/C0QRrTetnTeaeEscL"
    "PFwRIruPtYx5HV99SmB9Jaqd4JQxqJeoYKNYYIlYphIfADvBqFA5KZR768zbp1efvqqOZ0qO"
    "UyroUz27c57HnDoeZv/oaFfpo0jTsr1lK5371ety5beUvU/rlrrx1t7D7hBWjINXD5ixEV2M"
    "u2vsHHQAiYXholgwY+mgX8gE6BssAuG3cNbuh8ntjQ5T8+r9qs3VsS2DTIIJIKQRTKYNCPSZ"
    "adChcieUUzP0/ufdmWzNcP3bMdLs+ZiCSHb6AD79zUNfRY06+DGRhrc6koeu3fUu13EdUdPl"
    "Mz34Mdnx7MWgnJ4rB+ouKulqvxeLv5828iTwS3vhtVvJ5zZGBWK7P0+ubhPg2WoQ+fOdA2cZ"
    "XA94i+Z8bIm3D3sASC2CFgznw6o5w+lYNBtb+FJpPqgGr0TXODRetTAnmJEU82Yke1gNpigg"
    "UeRpvau3EyO/Ey2b0FZwfa92xiH8I7Up3utJpk73crMnGh+W4GXNnMJJIi7voiP20BUfv7t4"
    "e7PqyHiY6A/NYtIq8EdBH3dBh03fOKGBOZfZ6KIyB92+/FP+b0Gzs1Vz9mZ0Yh6mxxqL3Ffw"
    "lFiBf+PNwIEb9EsYVB6Rp7gxKrGkAhyn3ovBYHLyMdAcjAlhTGuKCW6MLh2IjgKd7zIoyr64"
    "hNLrAfMyEosx5CxO9vdOodka6Lug9A8XcqFD861iJxXE+HMJXCHrMdGkT36nRVrvXH/96Zy3"
    "lIN7o4+mXYR2N5l6MtvipMRpss176y82QXtKzZj1QgfdAq0onzPfxn2SfPZVtbDc4s4GTOwD"
    "DbcQCA966+Oa6yvXcpIC8zR/IAdsBdzGPh7UEqyb9TR71HavfLo+8Eig+jLXgGNFDLiGs0VV"
    "quXKlI3TXRn7FaoyNTCwtLyjTjFl3NOy/sh9sNO1b9fz6OW43LH9tU9SKVVS/NipqbXGoqFP"
    "H1xraup591Ny79hPri0telY+pYMvOUGB2Zo/AGUXgSz+KpA2d9IogjkYTHSpFiFWCwv5hVyR"
    "MIMMQzBNGaYyXFgGrDbNpkIi6QSagUVcVxjDuuVgqUxq6RQRdFTSaiX/MxiGk+N/blnY+r37"
    "H02aEBIrrPiDcml+UQ1eOLHkBi5VZwtt5hjF72qpz32cKnn9suyYTRlamDxcwQblbXnHiV22"
    "WO/SzWMB+1oN9B8nbqdUkYeUkxweTAw+ew+3lPIuyi/bWPvj6uTO8B3N+xzX/HDa9EFP7AmE"
    "Exi4YZlvcaEOrb0ESM+3/K3FZTaZxfKPYsBPS/dqe2PSnDDUrJjqt0pWyw99rqzy3tvZqit0"
    "FM/Pgz9fJxcxWXs+Z7Tkh1/3Ta3Gj6Ljqgp0981PQnzfVQVd2N6Du9RlUAXOVdYyrTy6XZ4T"
    "D7hd+cMWwIUWkfgHl1jrKRJPr7nDxStVCk1vhO8Q3RUqYkWSPykbkEuxjJow6pRbVkgI1tqr"
    "baiw1qrPLZQvbu2ow6T1VPWJ4p6qk9VVjT+3vrjr7vlNJRK0XV1P51DkTAUHmD5wXzKv7c5r"
    "ow4B8iXs9j/GQ5YvAbN9CaEdTmeCBpk9dR2OffAbh6mxYBldAivTvGxjqOzgwHea84mBu5H5"
    "Ctj3KN/fS7vZbsST7UYuoSkkEfOuqljSOq6znRJYyy1VeUYiYndi5LhzTKVPSCkWKK6BLjOG"
    "yZjan5M7fC6+cOK56Lvbm8oahS4KpIjti77jfOZ0lbWKF2rfz/afA0iba8/K44lnH4dASGbm"
    "x2VfvDrICQnMlvxBbYhtSxbSdHd2mQ9L1xbiJ/CZa5FSQi434lNJJ+pSgmxQQo3L1c68HuJr"
    "M5W/znXoUKllDF+w6OhaBZdT2x+L1uy/+Sy97GmmQYLR2IX+rQ1MO+Z0iJ+70NtigzPvDTMj"
    "eG5wVk/0gV8pmX+jcl9dUwLkQdgBo41d6ZvtTfLnykUvs3Ox059Er4uJ8zqleRoqrmJ/ayCB"
    "b6/KikHEGlxH0rCW1V6JR13JPrzLQhV2UnwC8Xc/CurcPz7ccxQiqUlBRxt+47mA3EzozDA6"
    "zIkH3Jv8Yc66iy0Atax4iODpFBKUfkay8tS5kKvFpxD4lVTCvKQYuW/Hke+3vXyX6Ru/KxYC"
    "O8u3Lqv3HEYoxpwm3S9WW+a8bLn/EQt53gbNMJjDBsFL0Sczbj9GtvmUe95uFShceaT5i8/g"
    "Y66xJp7OK27Ot0xvc7bF6AMvugB8Kdl65/GvFmnsnXyfuMuk1kB4VkedsgQV7NjOw5eB3+r9"
    "atMbnyHcy2UK8VRtiRHdvToxNX6xb54/LPR/iMXeDNdqE2qRJTzMOhgTdlnb/uGlH30putND"
    "VR5Z8stqX53Q2MWJCNyazCPCwo15ACncQl3Jkt3OtFb+bJxATiyfmDnev4m7PQONd7YIx/Dh"
    "Tw836d+TSjvR1mK5NjPWYivkGZdhyLWpZI+EKcIJR+S3LsHn/DkM1WMHrzRfcit8+Laq9MIu"
    "aSdokVoYpB+qR1vDvWIbJyUw4/IHV9bYQufNFjotQda+jadvx7g5VFOSrWtzweNer5WlazOp"
    "rh6Q5AANj8ob0wUXY15kQjVLennEfcsNB215W1u4nCazblRLE095C/m86Ji2U5yestD/BTXg"
    "u9hVWv6wWxda1weTy3XL/OkvaMSJCcyuCCwupsEvtd9rRmHLnjpb9jRrxxJj6TBIPQyaAEG8"
    "A+G5sn9CTKUj0BAhs6G6TqJgAlMIlPkDl3m+5+wpzgcGZkD+wF+xVc/9H/7Ksmu54rHnm+nK"
    "lVnCKKuiYAGr/GIc89fRJ8dPfXf3EZLMtMsvzTDRcTu34pxJRLDUxy+dW+87QjZvhZ1UOMl9"
    "0+NbTM69zfU97vjzGaufM9zrjLNvan/6Wpk6wQFoAMyIzEaQ+ZzUGHdTHZDiefx1MyZuVtZX"
    "nSSb2eULaD9/Tj57QCgtzUKhEyyH3vpFtNbWH2ZV1ItabswvcGRD28iRCx4vtK+POHXuPBIf"
    "xBt+/6WKoprGt9cPussOaYCfpehxX4me6swnNpwi8VpYCTpxIgJzLtKLot6Yd/73Dlb/1e6+"
    "oWPx5hbeE+JChjwhI/Rt1pW5WLXCN67DtcusE6QPbvDQtuHDW9nbTR95RkVnr1pp/jZSAlm7"
    "wnrbhoYdF5yfqSKg3PvqYxD91hDlVX2m9n5am069k4a+Bnf07eh8F/5FhJMT+JUSgHuVnZdv"
    "YN8ynKupcHdlZjpY8WrEby7hdjtpH8QbJ9gpc3/tXtJYYZWMZOays862PR34u04V7Vk+leFV"
    "DHQg5MfamfGXakZM9azj8Pf5su99XdrIKqs3G9ZhgmsCOcGA100ASsC/0nC9jlre+tk0/DAk"
    "ee0y0xGLHa2ivbt6eo8NCkX1yPnp2AyPYTCYBL8TYxauhUcYdkU2bw+2VgUfUCYqv4hTsz0N"
    "USuN+SjUtMJwg6wCs98HFrEp8KPGx8/jud8qhW8yQaRVhll9YyZpnKDAb50AlHN2Gm67sFOb"
    "ywRoOJacm2vMtgBob57N5eSU4hupr3FZmncHY1EiU9evwQcIe/2mbMgGElFbP658f7fzqFKF"
    "Z8OOkAP2a3RUTPXN70G+DDM5YYB5k1krpjZ/Tqek0QnIm+xdgHm4eHygKkG1RPlDsWU5122t"
    "8mO5WoflzDrdvmBfUKuEkVUkeoOZdE6SgHFml2gCqVe+Winrmugvxxgj/j5ugYqRrWarVxt/"
    "sWlItLgcIC/80V6hkOui2dHdPS+Y6eZDa/tHy2becW32IEd+Vj5gz0kN/IAIYHGMbVdC2IXN"
    "2VtSKIijtUh2NKQcLlTUmNQ468iy5AfvTXcdx/ttDS7/LIg2bVDWSEp3OIVJDtNBheorodUu"
    "JWFXlVkdOP+yU6tLGQUbeEPs9yuoSLU/o4i8YVD5IOnRjcPCz541Hmj8caFQSeekmuCBrzxj"
    "4d56GfdyJDi5gRsYgCMT2AbG6+8dBLOrrYrfcYGcXbopyXWUtdo2J+TrerOkUB16x5hYeTMb"
    "pwALL7JKZRdhVSi/3clUB0cbkQtYwWGDp8Qblw50Fp0aHNfbuWVL7PFwb3LZgUq7B8Xv3sDE"
    "fMklyXcstTkp/+flFrZ/MV3sN2Ji64nfoxHIHKaWLqFwEEKagiUxYwMV0AycSGxvh2GDlNyb"
    "A+SB2YpaQyclDkso1ZqKpsFAQih7eI++eQgnBDBP8x8u8vn966q9+fy5XtqLuA3aKdY2J7iC"
    "GHCzwn17h/aYC8mNNiQffefa7qDrYGEkIhR62MIWDxJ4biCgfXZnxte+DXl5D9/pTTQO37z1"
    "pr/XIfML8R0SXr1e+NwvuZEBgaqnTideMQQHOVgNgdsbgBGVbW/c/7Ytcah4+hnH2Vp1hVg8"
    "L0a8frWDGyUr5Qu8e1PU46rq8Ba5EJbhsRtocg7MLgmzHzlmb2x37E3GxRTricFz+gEIefOZ"
    "2uMSaz7KL+vPbQrrJHbzd98zyu24GcLZfGsIvCyjOq+Ny3rcAZmbhdseu1tmE16WNtoaBfZB"
    "EWFceQVnnVKUIoZLPd24vXVltx6cMBWSTO5Wi+Zu5A+59XIKJWsd6J51/17YnZOs34lsTXhP"
    "e97yDvbxiIrkQ9KMGq8/mhMKmJP5D6fsSwWTEUx8l1eDl5ValqmjLDEpSb5D73uNQBa0GZ9V"
    "e9CBem574l3PFq9poYuR/G+Kj9aSAgZRGnawPUb3UzYEWfLbZ7s8XnUsbPVhlTJuJWet5wIe"
    "cO9DqEdenAMFDIE3sgAsNrHVnn0c5EemcacSXTE51JxpGBJ1n3AjOsOTwayFJu3CzBBBdCKC"
    "iSQFQVAG8p6+3DOwulHUVOL6dmSPChMGMjtsHdLb1lbHiQG8tWU+g1AYiT8NSOcXCoMbUudM"
    "CzdRSCjtrBornqjzoCXai3y47cThE1aqZkFpB34QdkUEkjLzj2XkDryP6Ya0uY5XTVxT9u8e"
    "nWEk/IjfWoxtns4sO1IBiszH9rb6H7i+tzmwcM9Hnoj7+m+VNo9wc0ICMzNii6+ZxNDwJ0Cy"
    "7rk43YKXQmK9ZpqyOdK9lLPwW9tw5WFhYV68CeJqV+slpKr9eh8O5gVfD7L1rGvpPUm3Dcl8"
    "BynzsIbcKtrfM7l+n+KNxJVpJ+nrrjs0KFnmvEDyNEdK+GQ3rqj3QN4yTmvWaOZUN0Pg92X/"
    "sGlsy9+uiuBEQF06Itk3wtS8QnTiSzENloEQe/6hH4klZRGvX2dsMNxkODZacc7rQqzcQ7uV"
    "KRaF2dbK05gLZ3r8L+ruEb7xMK0g/aLBz7KfIT+/uznYZEhJu5kkZvRKaHLiAbMo/6Hmvtg4"
    "VsuMLaBOwyAQuI7CGBg3czj9WXJD71ezcSyUPwEKhRISwKhfIAgMpLJa3/HrA3nOK7+GwBzH"
    "H1xEZ4s1eTF55W2YzX00RQxDeD81aKze8pzPoFBoeDQmdhyDaOZBQsVmCu2TYjWlsC7K/LF2"
    "61ruoq4I21VCz4+s3Xw75pxl6K59W9I2NfY1GFro3/kS+5mOpX+zkcqW2vrWuA409HKJcA/8"
    "lGc+ELaHwRiARHrhvmxo+ny4X1tocwKNUbpELfezyi9OmqhebW4eVD5M/JCmqOzVadhL3cnX"
    "XndLv1iTR33Ioah89GFxUG5O2aMNHip91R/eTNXe2V85Pdoxw1CY6YJUNZi0H6gxGeegWwHc"
    "ggCk+5dCh/5lt1Svg7dy1/rii7aJWAWOcDW6EWwcHasHw7GfBLfIPG89nkw6j/jwJn/P/d38"
    "v4ri7V7bgKwkNeKXvcgc+LhWGfxqTS+3yeqYAh7OS6YrgNsOgN1+bIVmTzR6UIVm2Q7uzm0a"
    "uGJuuzJ1HU9/xPJSnjZCoKu43F7E1r1n7kJsRSwUI8paFS7dYxKowmonP+z3HL167h69GHLi"
    "/FDR077OYf2xCTKvX9uFPW+U2wzkLbuMpxmgfnlb43CNn+84IYEXVP7w+H/nwrvXXiZQR8Dw"
    "0A9lntKYPU72ygzMJ6sfzkK7nSudaba3ODL4YU3iz7jNVo0t3MifKH/vzqthyCfn78ZUOpdO"
    "iYY3Gvf5lyOuW3o5Dhb+8FxvPHlxrOx1Rkmw17bjO2b8Z0KjLadAG3uMlYKC8mM5aYHZkT9w"
    "kmw74vBXbkOsB0OtZiOhImKYSfbxm6pv75xKlEmalEhnZmxnIOuwpF5ZKrbHfIzKREWhkqQK"
    "KiR4dYoGownt6VjmWyYSVOzvyHVle/8eTh5gvkR00Ze0vDTJA+RL3P86G5n1JSwTWQdPrXNV"
    "VTVV89VvhWdryLd4W+7ICE37eDt1k+illULc6L08SdAbzUWp7gr0UStGw6u27d+lv6Tvh+UF"
    "1waAjvLt2fbtmHDVkdEz9K88x7x0LO2/Ru7nBATmSWZT7vlSwxrxs7sAeZJ9fxVY6kgisZ4s"
    "l3wdtM6KN7c/z7X92XBaRpZ8Sll8Rv7kxfPMqAwL+Js6u+pSFCzAwlM/vF5fQsBrI1KxKfHn"
    "2Uuy95YfHLfVU4y3yKxeEVtZ1D3zEP5wIDx0X8IDx61KVyXunC1Xet4+TD+1Wr3gtc5Ml9S3"
    "9qA3JU8q0jnhgXmVP+gCZ6/u+sVOzTFkPREqlYBFaCOQmAiMIKGUoJs/hCRNQewgkzBaDuyy"
    "bAKS1Gg6gRBDIBDR+RXQJAEMLR+cYvqeFGkCGsfm32VSUmaYTAgIsQ+nIriPh/NEbwUwk4IE"
    "sa+XofZfjAJkUjQXGwOaxs4k07HC9UjlBIgDQsABgSD0gMQmIaYIx6/ImBxHtP44MwoUdFEy"
    "5k1ZOWd9fQUwiyK2+Mj3aaCtgCyK89+uKc6ekexSLPPktvIk8B8iSgbUwbNFiLUqg/ZrvmUd"
    "GgrRTrNQWgc/Ag5rU5q5s+3rWahVMmKr7dHL6Y/0z3/aa2jp8MpfnZnKAN2jKb40nLTo48QC"
    "fuVnfsLEt1JfYCOANrMPHueuUMTTmZ1FCoZJMNgX16CylEMhcdL78Wma6zLGTIXMZD/llTSl"
    "l63bhC9ea1Jsu7Zv8H1/cdKng8Hb88ONIGtaTYNf+syoVV5r03ptedVjmmfDfY2EqQ1vPTno"
    "tAjAbMks3vx1M9CpR78A2RLbxXBPS6AzwRIJTAHHdlAtk8JAijBBg0jHKVAcP45Bg0kkQHH9"
    "GbR6Go1WR0FIUhggx1EmRDaBSQoNnILE7Qbd1lDi3+y3a/MSNMAv9eDmaExGbwIb9bOefXlu"
    "YbGwo2UKIiEZqcqugWWp3QKjXeYkD0ujCEZExnB8ehfecLJ5TF02NCd+g0PK8MbC7w2hdohA"
    "fLipzg1v21d3L734el64QcQg9IFyeAbshcESYMA9yPwulNwrAGzs4AKYesecivF0HeG3syIL"
    "NfigPXUrL9fCVZ1P9K5JedrbTfx+Fp+X2LTaM3O1V+VJTP8pSZqes4P+iEpgy4+ZNByi6uGy"
    "sttm6ek/qqtv3+uDy/ePtRskLwEGvPoBMGqw7YbNYvWj1JMEE3CcYoL5UV9IZYh6JgTnD2nQ"
    "QzClZghT20FeTUQEk5QziazHYnyZ9WMQiS9MTwYlLroF8riFCQY11WiNeSV8oiwBA3x66fz2"
    "69J4+XslVbYa2f2jLpywkqVGYQgshoGRJZRSx8GoY2QvQ3IsDenfOzDRwgBBhLlfEAi6PbUE"
    "Ye6nIC3YetmriUi/4zKdL6NBfRJyxkV2rZNL4ADve51fG0WuXWRAzsLpHz0X3LWNYDklR95C"
    "DS7+O7rHB4VMpmjaffdtuIPp6gckDA2abL3Plzu5nldwKk+lafI7T+tqBe3TfSK8oU2vZtDA"
    "or4nuIl/TZf8cd8CkyWmMhGAd5IAXCR2eeMfg3FYr5JzSnbPeocY7c11fr7CsyM8BYkJKYO2"
    "x9TOjFQNrdcQe0AIyuC6LnKhG6ob/XzXr0cPzOpCdl2y0LyUV0Q82C+ycga0r0x2PdXDgLAE"
    "FTDD8Af30dmGgbBohE7Ro3Xqo60SolMRRIyEIEFbl6BNoLRCCT+w+l/PoC/yiHOp+kFBjiMy"
    "d6Ikng4t8dDAz1EAbrB/WYaFO5hQurCwnVWg0KBfQlfg7EpE0CeL0hqfugu+UmmyfZufqbjO"
    "g5oFcYtS7hu8xR1lczhidQTa2IHuesv26W1b96/mNTVmUaDIVMXtbq1uS4kqMM8gshitxy1v"
    "/V4bD1tUHdkVtNlqPMi8S1mDBk9/A08lDXphxKkCkcOkyxu6eofzIhmOStka6LohBuqA33V1"
    "7kOZJbcEqq6k5thxo8Y8yv3bwBOa9/r8bwtjeowtEm7Ic9Z3tQDOGZ29xD1vtcuObJ8ApK1r"
    "Fy1pBIUOjoXUg3EBlCEI4uNH0hSW+qwghyqTdIKaA06QGahgUiH1TCQCwYwm15KCZpCCiHby"
    "TdRdHAPkx4Tl74seZlnjV6qaJTwT7l+WYALmF2YXSn6OKbyrAQVIVjew78cWsGQVY0E/jitN"
    "rfNL7TwXc9OvEq6AQ3NHEi9cYuwdbPCWSbbtq9S9oPh+Z99EVHkE+Iji3iR1pcHI3qkBlcO0"
    "s5+zY8oSbmadTBt5zq+4VsPvnJXtySXIgI//mt+C4v4HfgLS1QVfZ9k0axhQ5k7h2XMtBKl4"
    "V/7ZrFfVUcOEJ67mtA/q5WmmhsvkrizFtdSI47EYDaGHe4X63Rt2bzmqvKNNqBhDXMmvX2Tt"
    "m8TZEKEFcOio6CKNbGI/BZCwbmIrkRbf3K1STbFz17/hZTu0M+p2qW5VfInbllK60pf/UHMd"
    "od5rvaRU/NH27jy9QL0yknZNXnlV5NsPv5rCnaAfrWJHJjvK7dfJSknHadh3WdV4NDcvI1je"
    "Ob9+CTrgw8Dm6YRPXGkGpLOufxXc5/o9sE6OvnEEySLFwpbNso0KjutwmeYWTj53B/3zHJBr"
    "GvUkctWcdg4GJPfivm9jnvglv9Eyr9ESnSfx/uBO34ocnTXTz6T5r5VEMSCNtVp5qbZoztMt"
    "LYBTRkUXX7Ec4WsagOR2NTtsvJqKpSPrkbjLZAaNXDtGMY2hU2O562uxQoi0aEPMSaYp4Us0"
    "FBpmVCEjYKAwFI0gXxecAnlB4dBnX8BJpt9nvKfAoPMoDWUdIo3zkowWwHmi6MVAuLm4bxyQ"
    "1C5nE3m+oo2R7lELwALMAugt66EvBoQpcgDZkAa6EQsGk/zzMSCdEALK+GpG8xLPC3z01/wK"
    "RD68sRGQym78x+gvi67D64p9uF1afbm1QMv78RJy2/gUg1UzvD7c+SKzR+iAsMA+/sdRNjIT"
    "lObBp+fNdI8k7ZzJk1BY5R3e5Hm+RE6jSl2TvLdZ+9M0aH2JttOjYDjnPWwtgMND0YtsmQzk"
    "+t9iY2fni7urlrUWlDtUOsgbAkHoY8lkBhOURCTXzg4VMtAnTEEuMuvhBaYMJhWaJG3TgdRF"
    "iFyfnQ94BwUqg/motHwiTgiAuj31tkNsTS4sQQTMO8y+L/P29PELQWCd+faLOcQAjJUDibLy"
    "n/wccAwTPI7F6JeQGeAGLF8+w5ynPhYmikBSv5riA6BeTTAxxLt8cBKWYEOYYpLBDaTr0flk"
    "UNtOPQ0bO7ElppMDnBOKWgxujKtiDYBsg/FfNw1AODioVkXkWCrtYmoTPnUAWpJ8VOvO4KHt"
    "fLxlvOdcawIx+bjmNfaV7ybp8Y3G5q2mk1ya32S2OAZvqFmC4P+bSdj0dzfHEp+1/pR17xzT"
    "YC9ieuFhpdbIuFCCtFG1T1Ze6+CxMfVBi0rdfTa9O3OkNgSo1u/wSdBtfDhDe4kLcT0oeOdc"
    "4X6XaxeJOd+H1jpPkDVpJ3R3xCaF3FqCDphR+IP4zJbWhZZz9ftzd6us6EMVCiEiTWbycnDF"
    "5UfXl1oJi4E2dt2Y4mlQ+TTk+NbCV7FLOKihtvc6uu1tyRnc+TerVl3trWF2fAs3kTxUeZW0"
    "MXfk5rTLx7B+o3evek5EWjt5mEeCHLbovL2dffLmEpzAR4f+ochu/IfIzn2zQTlXro+oVZlF"
    "5k3ttsMkW2rg6TibzNDGLRNSB1QkS1orGjIc8XpyukaDPdHZa1VFK/cFdfGZy2wlSjxIDlTw"
    "3xJjMW0cAUGiNE4Zgu8uMZId4DTRPzjS/ldK680gMmH1xCYsAos5SDAil7CkiAxuSsMqYk5G"
    "2xBKmeDBaFx7Le2wsA5JK2zfd6I0OTiH+kbLKIPsRwZ5JhljKWEzoLrodiJo76VlTOsLVpwT"
    "1rT+YJIowNj+74P6CNwrWiclDgkax7r13qPSsULQcc9BjH8IIvtXLUL3TAEVLEGJwGIkQXe3"
    "ab4y3JY8ssRTA59FNv/URwVe7QCktn8f8TLXIgJNODYQ6rX82F6ydwp1d6qMm19gRXr+QF3s"
    "NtewkqQjyWYT0Ps+KpdF9V+omzwoUjR7U98iLxl0szBs5tEhbXia2GBpx5uNsmcSGqLL7/yC"
    "0o4ZeOEZzseXQARuKOZfpaktTb/3PTb/mn3HEt0oCp2VB+JDQEP1SAxmJho0iMWNUmhYCqMW"
    "nBCdaoMpGYqGoIiUC7Hvf0EaSfrETrWw/BOoAVAT7oUs5et1LAgkU6t/xp+/pGwJIODzyf5b"
    "arutvYAVA1lpoPWyDPcRrMDGVps+vEyBkCne2g6Fu3cqsnlFsh1uy9bCr57Trr72XOGEvtUX"
    "TF9hYS9r+mKKA4WLviROPS5/l6txR1ZfFHzgSfESTMCnj88zbXVCtANSLdfF4h0vbbblEaum"
    "dj2maEWKe1PLMjOxO/6BZxvRwSlZg7HN4xHW1+rsm2x0z59uNHavOB5cFSd0yiBnIgt+Msk8"
    "1vs68fB+R9/baqEzQj0G6m2wFUEazuAaoyWyW4CzRv8gNLCzW/ZAuYjZOZ6CgqBxJgzXRxtj"
    "yqGwcYO+eN/E2enCsLqJueHCicbc0SD6T2gCzDFqgMhlYBakg8IyX4FAVWcMToa7ungtQfI/"
    "Nxcco2aaGVRWegRLwCJ4SN2zBRZQPRhnwoilFkwiRZixnYlUxgA2fwoJzl8JEYkCJeUS4YQb"
    "kNfPqO8nTRlErtiGHOpPqpiWLqhjD+gFpHsLSOQnK7eRXfbt9t7P00sQAjMYfxAQ/yW8G9Kq"
    "5rJbj+yYLFEjyaKMZbu0Uqzzi0lyKqeUy0+r9Vs3B6+UeSr6vKniZFC4goD+vhlbna/XzXlC"
    "ucSzMjvWRQsc+qxUcvXUuubEW7u6w76Lm2ur7bzgFmK8BBswUyGy+JJVp4nfBCS8fx0/MyD1"
    "xAQkIhrDx5JeDEt8UWRDUAOMSSBITzXTOhENH5noHBl+qxaD/BIe+hApioHljm2PGjMH+cPg"
    "0aDnPyB+mi0zzB9EEGiViG7/RwqDtAQYMEfxBxGRrb26bDDHaAqdMg1OgOJ8KV6oX2AUk1Qf"
    "nUHQ14rCnJuBrOo0gcKgUIIVBYaAgX6FGfYK+bn4L/HgwEzDH4Q9tvxu+uctr67DeEysf90y"
    "CSX0ITmn0qYy0zYRJ0FrXfKInoZ/lPbsVb0kcGIx9E69xnFG/oNL2WPlSdjCNJNd1u/lLznp"
    "Bw0d+6LwK4Khkiwsy/vO8OKxfU5LWCKAg07/IFr8q7C8rW1h6uz2o92pg3wBw3C0+K9nii63"
    "rq1H1WhiHMfqRjSiFKKOpVecdP6e8Bh/Ap/1tHmdqHzvpdu8cpZR9utQdH3aN6aj4MsyD1Pm"
    "zlf3vZeA+p/XJ9jq68BW3yoBiiM8nv6qFFPU/6DbyY3qhKntruNG3ml6kxUlkgizzKhYmXqm"
    "XsL1CuFZZNWkdVDdwdogc+NLD/ab7HopMA2ftjskHh6Ah4kEFF5eAgi4nQAIxJZeZ/Zt0dkb"
    "eAjLLvGzga12zxAoSGmW9Tg8WxVF31xcbHhs6qnjI/ULQ2YZv94/b36b835knSV1XS4+0UdN"
    "yvoYdtO1r/zBuUJEVMODjuYpnvpryqmjhamcc4u0AE4//QMutuoufKHm6vqFfvzPs9fSWny5"
    "t5VtP1LKerHg3vc9ArupeMalgxEfLE/sid7xSKkmSOl0xWuJcK+G2Hd29x+feZ6KTaWZPGvT"
    "GeacB6YFcNIpajFM3PS9/RBQfFNjG4ixMwUFp6m1simSoxImvTpTlKLI9XHWfLa1sLAEqBXU"
    "ZyxWAgT/oEjZEmlbtcQDA/+KEoB/e7bnZo8FTptPXzVnS8NpAq6t7k/G4KqqqGHw2dpuvc/R"
    "MsYrRFXivA8hjDRTIwnXUQG6etGVmGdfr/evCC/E19zsN5C5lRrUemYK8vqY3gN0Ta7vElDA"
    "aw8Ay/rscLb4Da3Rs7WuWYkZiHacgsWJ4hjWXNzREWOxdDkhE0Tjx0C3WhoNB26QJveBFJGg"
    "ESapjEm6w8QeJA8eJoK+QSx6znTTdJZAAeYE/kMKsXqxSRQ997URQSIaOmYu434vKUd1WTbH"
    "wbJQF5OyJyRrTNqizjRZPKij26RzWOQjxavg08ecF71j56vP17vmdsK+HbVKfcDTcFAnYUos"
    "rGMJIuCnEgB9GzuKrWOfjS30c9mrucZ2YSyHvDD8TyMCSLamDcjeTad+dq4rRpecy/G3sDhU"
    "kWvYPnmn2jWv4JAyDaRRv07Zg/lde3eMbvpBy1WNQ1+5XpWq75bWtpFaAgt4VeEPD8aMF2dc"
    "UXglKQwmGBVAGYh17EM0jqKmkHGTUpgZSqkRNxPk+QuagCS4vaKNgeKY5mOmeJNUGsi9Rs/j"
    "VbSI5BIEwEzAH1R7/jXtY0Na2VzT9UO5AftSkbYybnNzGcn6ArQdYYpCN7aAxAp88v2M1Vdd"
    "pu8Gx+dgu+UNL+EMTKNa9fd+vr+y+p5EFaWh2nwgu/OLwNh3lfJVNj+EJgPwzNxHUmVtyztl"
    "NT8scWoEcOjof6hobVqsgk/B6rEJWJJr7Ri1NoaVFnHPpUXWMQXjSJj5FyLKl+w3g6z96Ngu"
    "2CxA7hM8K+cPXY/oicb4PTDFmTjKgW9BfMThTPDzX0i/X8xmIuhavt7gzdTSf7xa/wfeUnmO";


/*---------------------------------------------------------------------*/
/*                      Auto-generated deserializer                    */
/*---------------------------------------------------------------------*/
/*!
 *  \brief   l_bootnum_gen3()
 *
 * \return   pixa  of labeled digits
 *
 * <pre>
 * Call this way:
 *      PIXA  *pixa = l_bootnum_gen3();   (C)
 *      Pixa  *pixa = l_bootnum_gen3();   (C++)
 * </pre>
 */
PIXA *
l_bootnum_gen3(void)
{
l_uint8  *data1, *data2;
l_int32   size1;
size_t    size2;
PIXA     *pixa;

        /* Unencode selected string, uncompress it, and read it */
    data1 = decodeBase64(l_strdata_0, strlen(l_strdata_0), &size1);
    data2 = zlibUncompress(data1, size1, &size2);
    pixa = pixaReadMem(data2, size2);
    lept_free(data1);
    lept_free(data2);
    return pixa;
}

