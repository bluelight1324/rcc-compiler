# RCC Regression Test Runner
# Usage: powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 [options]
# Options:
#   -Filter <pattern>  Only run tests whose name contains pattern
#   -Verbose           Show compiler stderr for failed tests
param(
    [string]$Filter  = "",
    [switch]$Verbose = $false
)

$RCC     = "i:\rcc\build\Release\rcc.exe"
$TestDir = "i:\rcc\tests\unit"
$TmpAsm  = "$env:TEMP\rcc_test.asm"
$TmpOut  = "$env:TEMP\rcc_test_stdout.txt"
$TmpErr  = "$env:TEMP\rcc_test_stderr.txt"

if (-not (Test-Path $RCC)) {
    Write-Host "ERROR: rcc.exe not found at $RCC" -ForegroundColor Red
    Write-Host "Run: cmake --build i:\rcc\build --config Release"
    exit 1
}

# ─── Test Definitions ────────────────────────────────────────────────────────
# Each entry is a hashtable with:
#   name         : short test name (used in output)
#   file         : source file under tests/unit/
#   expect_exit  : expected rcc exit code (default 0 = compile success)
#   must_warn    : substring that must appear in stderr (empty = don't check)
#   must_not_warn: substring that must NOT appear in stderr (empty = don't check)
$Tests = @(
    # Core language
    @{ name="fibonacci";            file="test_fibonacci.c"            }
    @{ name="primes";               file="test_primes.c"               }
    @{ name="gcd";                  file="test_gcd.c"                  }
    @{ name="pointers";             file="test_pointers.c"             }
    @{ name="sorting";              file="test_sorting.c"              }
    @{ name="switch";               file="test_switch.c"               }
    @{ name="bitops";               file="test_bitops.c"               }
    @{ name="comma_op";             file="test_comma_op.c"             }
    @{ name="nested_struct";        file="test_nested_struct.c"        }
    @{ name="nested_deep";          file="test_nested_deep.c"          }
    @{ name="struct_byval";         file="test_struct_byval.c"         }
    @{ name="multi_array";          file="test_multi_array.c"          }
    @{ name="multi_dim";            file="test_multi_dim.c"            }
    @{ name="multi_dim_3d";         file="test_multi_dim_3d.c"         }

    # C99
    @{ name="for99";                file="test_for99.c"                }
    @{ name="vla";                  file="test_vla.c"                  }
    @{ name="varargs";              file="test_varargs.c"              }
    @{ name="varargs_simple";       file="test_varargs_simple.c"       }
    @{ name="stdarg_standard";      file="test_stdarg_standard.c"      }

    # C11
    @{ name="static_assert";        file="test_static_assert.c"        }

    # C23
    @{ name="binary_literals";      file="test_binary_literals.c"      }
    @{ name="c23_simple";           file="test_c23_simple.c"           }
    @{ name="c23_cpp";              file="test_c23_cpp.c"              }
    @{ name="c23_phase1";           file="test_c23_phase1.c"           }
    @{ name="c23_phase1_complete";  file="test_c23_phase1_complete.c"  }
    @{ name="unnamed_params";       file="test_unnamed_params.c"       }
    @{ name="preproc_ops";          file="test_preproc_ops.c"          }
    @{ name="va_args_macro";        file="test_va_args_macro.c"        }

    # Tier-1/2 feature probes
    @{ name="tier1_simple";         file="test_tier1_simple.c"         }
    @{ name="tier1_simple_check";   file="test_tier1_simple_check.c"   }
    @{ name="tier1_debug";          file="test_tier1_debug.c"          }
    @{ name="tier1_features";       file="test_tier1_features.c"       }
    @{ name="tier2_probe";          file="test_tier2_probe.c"          }

    # Memory safety (v4.x)
    @{ name="v4_use_after_free";    file="test_v4_use_after_free.c"    }
    @{ name="v4_double_free";       file="test_v4_double_free.c"       }
    @{ name="v4_memory_leak";       file="test_v4_memory_leak.c"       }
    @{ name="v4_borrow_check";      file="test_v4_borrow_check.c";
       expect_exit=1; must_warn="Cannot write through"               }
    @{ name="var_keyword";          file="test_var_keyword.c"          }
    @{ name="var_safety";           file="test_var_safety.c"           }
    @{ name="auto_memory";          file="test_auto_memory.c"          }
    @{ name="auto_free_basic";      file="test_auto_free_basic.c"      }
    @{ name="comprehensive_memory"; file="test_comprehensive_memory.c" }
    @{ name="early_return";         file="test_early_return.c"         }
    @{ name="ownership_transfer";   file="test_ownership_transfer.c"   }
    @{ name="phase_b";              file="test_phase_b.c"              }

    # Task 72.9 Section 2 — new features
    @{ name="bitfields";            file="test_bitfields.c"            }
    @{ name="nodiscard";            file="test_nodiscard.c";
       must_warn="nodiscard"                                           }

    # Task 72.22 — v4.7.0 C11 Phase 1
    @{ name="c11_phase1";           file="test_c11_phase1.c"            }

    # Task 72.24 — v4.8.0 C11 Phase 2+3
    @{ name="c11_phase2";           file="test_c11_phase2.c";
       must_warn="cannot be enforced"                                    }
    # c11_phase3: _Thread_local warning removed (now emits to _TLS segment without warning)
    @{ name="c11_phase3";           file="test_c11_phase3.c"             }

    # Task 72.26 — v4.9.0 C11 Phase 4
    @{ name="c11_phase4";           file="test_c11_phase4.c"            }

    # Task 72.31B — v5.1.0 C11 Comprehensive (atomic, alignment, string init, threads API)
    @{ name="c11_comprehensive";    file="test_c11_comprehensive.c"     }

    # Task 7.41 — C17 compliance
    @{ name="c17_version";          file="test_c17_version.c"           }

    # Task 7.43 — C compiler improvements (typeof_unqual, __has_c_attribute, #pragma once)
    @{ name="typeof_unqual";        file="test_typeof_unqual.c"         }
    @{ name="has_c_attribute";      file="test_has_c_attribute.c"       }
    @{ name="pragma_once";          file="test_pragma_once.c"           }

    # Task 7.44 — C23 compliance (grammar, stdbit, stdckdint, _BitInt, __has_embed, [[noreturn]])
    @{ name="c23_grammar";          file="test_c23_grammar.c"           }
    @{ name="c23_stdbit";           file="test_c23_stdbit.c"            }
    @{ name="c23_stdckdint";        file="test_c23_stdckdint.c"         }
    @{ name="c23_bitint";           file="test_c23_bitint.c"            }
    @{ name="c23_has_embed";        file="test_c23_has_embed.c"         }
    @{ name="c23_noreturn_warn";    file="test_c23_noreturn_warn.c";
       must_warn="may return"                                            }

    # Task 7.38 — C11 100% compatibility test suite
    @{ name="c11_stdtypes";         file="test_c11_stdtypes.c"          }
    @{ name="c11_math";             file="test_c11_math.c"              }
    @{ name="c11_inttypes";         file="test_c11_inttypes.c"          }
    @{ name="c11_fenv";             file="test_c11_fenv.c"              }
    @{ name="c11_generics";         file="test_c11_generics.c"          }
    @{ name="c11_declarations";     file="test_c11_declarations.c"      }
    @{ name="c11_string_lib";       file="test_c11_string_lib.c"        }

    # Task 72.20 — v4.6.0 borrow-checker (W2b)
    @{ name="borrow_write_ok";      file="test_borrow_write_ok.c"      }
    @{ name="borrow_write_conflict"; file="test_borrow_advanced.c";
       expect_exit=1; must_warn="Cannot write through"               }

    # Task 72.17/18 — v4.5.0 features
    @{ name="const_prop_inc";       file="test_const_prop_inc.c"       }
    @{ name="hex_float";            file="test_hex_float.c"            }
    @{ name="char8_t";              file="test_char8_t.c"              }
    @{ name="has_include";          file="test_has_include.c"          }
    @{ name="embed";                file="test_embed.c"                }
    @{ name="fallthrough_attr";     file="test_fallthrough_attr.c";
       must_not_warn="implicit fall-through"                           }
    @{ name="unused_var";           file="test_unused_var.c";
       must_warn="unused variable"                                     }

    # Task 7.47 — Priority 1-4: _Noreturn expansion, dead-code elimination
    @{ name="noreturn_keyword";     file="test_noreturn_keyword.c"     }
    @{ name="dead_code_elim";       file="test_dead_code_elim.c"       }

    # Task 7.49 — C23 compliance: typed enum, constexpr float
    @{ name="typed_enum";           file="test_typed_enum.c"           }
    @{ name="constexpr_float";      file="test_constexpr_float.c";
       must_not_warn="must have constant initializer"                   }

    # Task 7.53 — Session A-D improvements
    @{ name="thread_local";         file="test_thread_local.c"         }

    # Task 7.55 — P1.3: _Generic variable dispatch via var_types_ map
    @{ name="generic_var";          file="test_generic_var.c"          }

    # Task 7.57 — 100% C23 compliance: embed macros, u8 char, typeof decl, constexpr float,
    #             memalignment, timegm
    @{ name="c23_complete";         file="test_c23_complete.c"         },

    # Task 7.581 — Final C23 gaps: memccpy, char8_t unsigned, constexpr float→int,
    #              typeof(type-keywords/pointers), delimited escapes, constexpr arrays
    @{ name="c23_final";            file="test_c23_final.c"            },

    # Task 7.583 — Remaining C23 gaps: __STDC_IEC_60559_BFP__, #embed parameters,
    #              string delimited escapes, constexpr struct folding, typeof_unqual
    @{ name="c23_complete2";        file="test_c23_complete2.c"        }

    # Task 8.2 — Quality improvements: static fn-ptr init, constexpr enforcement,
    #            _Noreturn deprecation, VLA/array typeof decay
    @{ name="static_fnptr";         file="test_static_fnptr.c"        }
    @{ name="constexpr_enforce";    file="test_constexpr_enforce.c"   }
    @{ name="noreturn_deprecated";  file="test_noreturn_deprecated.c" }  # warn fires only with -std=c23
    @{ name="vla_typeof";           file="test_vla_typeof.c"          }

    # Task 8.4 — C23 library gaps: limits.h/stdint.h widths, strfromd, mbrtoc8/c8rtomb,
    #            roundeven/fromfp, locale.h, wcsdup
    @{ name="c23_lib";              file="test_c23_lib.c"             }

    # Task 8.41 — C23 library gaps (round 2): setjmp.h, signal.h, free_sized,
    #             nextafter/nextup/nextdown, roundevenl, fromfpl, timespec_getres
    @{ name="c23_lib2";             file="test_c23_lib2.c"            }

    # Task 8.42 — Final C23 gaps: iso646.h, __STDC_UTF_8__, va_start single-arg,
    #             fromfp proper rounding, mbrtoc8/c8rtomb UTF-8 passthrough
    @{ name="c23_final2";           file="test_c23_final2.c"          },

    # Task 8.5 Sprint A — struct pointer stride, unsigned cmp/div/shift,
    #                      GNU extension stubs (__attribute__, __builtin_expect, etc.)
    @{ name="sprint_a";             file="test_sprint_a.c"            }

    # Task 8.6 Sprint B-E — float XMM args, multi-level ptr deref, struct return,
    #                        sign comparison warning, implicit fn error (4.3/4.4/B/E)
    @{ name="sprint_b_e";           file="test_sprint_b_e.c"          }

    # Task 8.8 Sprint F — struct arg passing (<=8B/>>8B by value), large struct return
    #                      (>16B hidden pointer), float vararg fix, #pragma pack
    @{ name="sprint_f";             file="test_sprint_f.c"            }

    # Task 8.8 Sprint F — 4.2: Unused static function warning
    @{ name="unused_static_fn"; file="test_unused_static_fn.c"; must_warn="defined but not used" }
)

# ─── Run Tests ───────────────────────────────────────────────────────────────
$pass    = 0
$fail    = 0
$skip    = 0
$failed  = @()

foreach ($t in $Tests) {
    $name = $t.name
    if ($Filter -ne "" -and $name -notlike "*$Filter*") { $skip++; continue }

    $src = Join-Path $TestDir $t.file
    if (-not (Test-Path $src)) {
        Write-Host "SKIP  $name  (file not found)" -ForegroundColor DarkYellow
        $skip++; continue
    }

    $expectExit  = if ($t.ContainsKey('expect_exit'))    { $t.expect_exit     } else { 0  }
    $mustWarn    = if ($t.ContainsKey('must_warn'))       { $t.must_warn       } else { "" }
    $mustNotWarn = if ($t.ContainsKey('must_not_warn'))   { $t.must_not_warn   } else { "" }

    $proc = Start-Process -FilePath $RCC `
        -ArgumentList "`"$src`" -o `"$TmpAsm`"" `
        -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $TmpOut `
        -RedirectStandardError  $TmpErr
    $exitCode = $proc.ExitCode
    $stderr   = (Get-Content $TmpErr -Raw -ErrorAction SilentlyContinue) + ""

    $ok   = $true
    $note = ""

    if ($exitCode -ne $expectExit) {
        $ok = $false; $note = "exit=$exitCode (expected $expectExit)"
    } elseif ($mustWarn -ne "" -and $stderr -notlike "*$mustWarn*") {
        $ok = $false; $note = "expected '$mustWarn' in stderr"
    } elseif ($mustNotWarn -ne "" -and $stderr -like "*$mustNotWarn*") {
        $ok = $false; $note = "unexpected '$mustNotWarn' in stderr"
    }

    if ($ok) {
        $pass++
        Write-Host "PASS  $name" -ForegroundColor Green
    } else {
        $fail++
        $failed += $name
        Write-Host "FAIL  $name  -- $note" -ForegroundColor Red
        if ($Verbose -and $stderr -ne "") {
            Write-Host ($stderr -split "`n" | ForEach-Object { "    | $_" } | Out-String).TrimEnd() -ForegroundColor Yellow
        }
    }
}

# ─── Summary ─────────────────────────────────────────────────────────────────
$total = $pass + $fail
Write-Host ""
Write-Host ("=" * 60)
if ($fail -eq 0) {
    Write-Host "ALL $pass / $total PASSED" -ForegroundColor Green
} else {
    Write-Host "$pass PASS, $fail FAIL  (of $total run)" -ForegroundColor Red
    Write-Host "Failed:" -ForegroundColor Red
    $failed | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
}
if ($skip -gt 0) { Write-Host "  ($skip skipped by filter or missing)" }
Write-Host ("=" * 60)
exit $fail
