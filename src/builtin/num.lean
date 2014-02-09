-- Copyright (c) 2014 Microsoft Corporation. All rights reserved.
-- Released under Apache 2.0 license as described in the file LICENSE.
-- Author: Leonardo de Moura
import macros
import subtype
import tactic
using subtype

namespace num
theorem inhabited_ind : inhabited ind
-- We use as the witness for non-emptiness, the value w in ind that is not convered by f.
:= obtain f His, from infinity,
   obtain w Hw, from and_elimr His,
      inhabited_intro w

definition S := ε (inhabited_ex_intro infinity) (λ f, injective f ∧ non_surjective f)
definition Z := ε inhabited_ind (λ y, ∀ x, ¬ S x = y)

theorem injective_S      : injective S
:= and_eliml (exists_to_eps infinity)

theorem non_surjective_S : non_surjective S
:= and_elimr (exists_to_eps infinity)

theorem S_ne_Z (i : ind) : S i ≠ Z
:= obtain w Hw, from non_surjective_S,
     eps_ax inhabited_ind w Hw i

definition N (i : ind) : Bool
:= ∀ P, P Z → (∀ x, P x → P (S x)) → P i

theorem N_Z : N Z
:= λ P Hz Hi, Hz

theorem N_S {i : ind} (H : N i) : N (S i)
:= λ P Hz Hi, Hi i (H P Hz Hi)

theorem N_smallest : ∀ P : ind → Bool, P Z → (∀ x, P x → P (S x)) → (∀ i, N i → P i)
:= λ P Hz Hi i Hni, Hni P Hz Hi

definition num := subtype ind N

theorem inhab : inhabited num
:= subtype_inhabited (exists_intro Z N_Z)

definition zero : num
:= abst Z inhab

theorem zero_pred : N Z
:= N_Z

definition succ (n : num) : num
:= abst (S (rep n)) inhab

theorem succ_pred (n : num) : N (S (rep n))
:= have N_n : N (rep n),
     from P_rep n,
   show N (S (rep n)),
     from N_S N_n

theorem succ_inj {a b : num} : succ a = succ b → a = b
:= assume Heq1 : succ a = succ b,
   have Heq2 : S (rep a) = S (rep b),
      from abst_inj inhab (succ_pred a) (succ_pred b) Heq1,
   have rep_eq : (rep a) = (rep b),
      from injective_S (rep a) (rep b) Heq2,
   show a = b,
      from rep_inj rep_eq

theorem succ_nz (a : num) : ¬ (succ a = zero)
:= assume R : succ a = zero,
   have Heq1 : S (rep a) = Z,
      from abst_inj inhab (succ_pred a) zero_pred R,
   show false,
      from absurd Heq1 (S_ne_Z (rep a))

add_rewrite num::succ_nz

theorem induction {P : num → Bool} (H1 : P zero) (H2 : ∀ n, P n → P (succ n)) : ∀ a, P a
:= take a,
   let Q := λ x, N x ∧ P (abst x inhab)
   in have QZ : Q Z,
        from and_intro zero_pred H1,
      have QS : ∀ x, Q x → Q (S x),
        from take x, assume Qx,
               have Hp1 : P (succ (abst x inhab)),
                 from H2 (abst x inhab) (and_elimr Qx),
               have Hp2 : P (abst (S (rep (abst x inhab))) inhab),
                 from Hp1,
               have Nx : N x,
                 from and_eliml Qx,
               have rep_eq : rep (abst x inhab) = x,
                 from rep_abst inhab x Nx,
               show Q (S x),
                 from and_intro (N_S Nx) (subst Hp2 rep_eq),
      have Qa : P (abst (rep a) inhab),
        from and_elimr (N_smallest Q QZ QS (rep a) (P_rep a)),
      have abst_eq : abst (rep a) inhab = a,
        from abst_rep inhab a,
      show P a,
        from subst Qa abst_eq

theorem induction_on {P : num → Bool} (a : num) (H1 : P zero) (H2 : ∀ n, P n → P (succ n)) : P a
:= induction H1 H2 a

theorem sn_ne_n (n : num) : succ n ≠ n
:= induction_on n
     (succ_nz zero)
     (λ (n : num) (iH : succ n ≠ n),
        not_intro
          (assume R : succ (succ n) = succ n,
              absurd (succ_inj R) iH))

set_opaque num  true
set_opaque Z    true
set_opaque S    true
set_opaque zero true
set_opaque succ true

definition lt (m n : num) := ∃ P, (∀ i, P (succ i) → P i) ∧ P m ∧ ¬ P n
infix 50 < : lt

theorem lt_elim {m n : num} {B : Bool} (H1 : m < n)
                (H2 : ∀ (P : num → Bool), (∀ i, P (succ i) → P i) → P m → ¬ P n → B)
                : B
:= obtain P Pdef, from H1,
   H2 P (and_eliml Pdef) (and_eliml (and_elimr Pdef)) (and_elimr (and_elimr Pdef))

theorem lt_intro {m n : num} {P : num → Bool} (H1 : ∀ i, P (succ i) → P i) (H2 : P m) (H3 : ¬ P n) : m < n
:= exists_intro P (and_intro H1 (and_intro H2 H3))

set_opaque lt true

theorem lt_nrefl (n : num) : ¬ (n < n)
:= not_intro
     (assume N : n < n,
      lt_elim N (λ P Pred Pn nPn, absurd Pn nPn))

add_rewrite num::lt_nrefl

theorem lt_succ {m n : num} : succ m < n → m < n
:= assume H : succ m < n,
   lt_elim H
     (λ (P : num → Bool) (Pred : ∀ i, P (succ i) → P i) (Psm : P (succ m)) (nPn : ¬ P n),
        have Pm : P m,
          from Pred m Psm,
        show m < n,
          from lt_intro Pred Pm nPn)

theorem not_lt_zero (n : num) : ¬ (n < zero)
:= induction_on n
      (show ¬ (zero < zero),
         from lt_nrefl zero)
      (λ (n : num) (iH : ¬ (n < zero)),
         show ¬ (succ n < zero),
           from not_intro
                  (assume N : succ n < zero,
                   have nLTzero : n < zero,
                     from lt_succ N,
                   show false,
                     from absurd nLTzero iH))

theorem zero_lt_succ_zero : zero < succ zero
:= let P : num → Bool := λ x, x = zero
   in have Ppred : ∀ i, P (succ i) → P i,
        from take i, assume Ps : P (succ i),
               have si_eq_z : succ i = zero,
                  from Ps,
               have si_ne_z : succ i ≠ zero,
                  from succ_nz i,
               show P i,
                  from absurd_elim (P i) si_eq_z (succ_nz i),
      have Pz : P zero,
        from (refl zero),
      have nPs : ¬ P (succ zero),
        from succ_nz zero,
      show zero < succ zero,
        from lt_intro Ppred Pz nPs

theorem succ_mono_lt {m n : num} : m < n → succ m < succ n
:= assume H : m < n,
   lt_elim H
     (λ (P : num → Bool) (Ppred : ∀ i, P (succ i) → P i) (Pm : P m) (nPn : ¬ P n),
        let Q : num → Bool := λ x, x = succ m ∨ P x
        in have Qpred : ∀ i, Q (succ i) → Q i,
             from take i, assume Qsi : Q (succ i),
                    or_elim Qsi
                      (assume Hl : succ i = succ m,
                         have Him : i = m, from succ_inj Hl,
                         have Pi  : P i,   from subst Pm (symm Him),
                           or_intror (i = succ m) Pi)
                      (assume Hr : P (succ i),
                         have Pi  : P i,   from Ppred i Hr,
                           or_intror (i = succ m) Pi),
           have Qsm   : Q (succ m),
             from or_introl (refl (succ m)) (P (succ m)),
           have nQsn  : ¬ Q (succ n),
             from not_intro
               (assume R : Q (succ n),
                  or_elim R
                    (assume Hl : succ n = succ m,
                       absurd Pm (subst nPn (succ_inj Hl)))
                    (assume Hr : P (succ n), absurd (Ppred n Hr) nPn)),
           show succ m < succ n,
             from lt_intro Qpred Qsm nQsn)

theorem lt_to_lt_succ {m n : num} : m < n → m < succ n
:= assume H : m < n,
   lt_elim H
     (λ (P : num → Bool) (Ppred : ∀ i, P (succ i) → P i) (Pm : P m) (nPn : ¬ P n),
        have nPsn : ¬ P (succ n),
          from not_intro
            (assume R : P (succ n),
               absurd (Ppred n R) nPn),
        show m < succ n,
          from lt_intro Ppred Pm nPsn)

theorem n_lt_succ_n (n : num) : n < succ n
:= induction_on n
     zero_lt_succ_zero
     (λ (n : num) (iH : n < succ n),
        succ_mono_lt iH)

theorem lt_to_neq {m n : num} : m < n → m ≠ n
:= assume H : m < n,
   lt_elim H
     (λ (P : num → Bool) (Ppred : ∀ i, P (succ i) → P i) (Pm : P m) (nPn : ¬ P n),
       not_intro
         (assume R : m = n,
            absurd Pm (subst nPn (symm R))))

theorem eq_to_not_lt {m n : num} : m = n → ¬ (m < n)
:= assume Heq : m = n,
     not_intro (assume R : m < n, absurd (subst R Heq) (lt_nrefl n))

theorem zero_lt_succ_n {n : num} : zero < succ n
:= induction_on n
     zero_lt_succ_zero
     (λ (n : num) (iH : zero < succ n),
        lt_to_lt_succ iH)

add_rewrite num::zero_lt_succ_n

theorem lt_succ_to_disj {m n : num} : m < succ n → m = n ∨ m < n
:= assume H : m < succ n,
   lt_elim H
     (λ (P : num → Bool) (Ppred : ∀ i, P (succ i) → P i) (Pm : P m) (nPsn : ¬ P (succ n)),
        or_elim (em (m = n))
          (assume Heq : m = n, or_introl Heq (m < n))
          (assume Hne : m ≠ n,
             let Q : num → Bool := λ x, x ≠ n ∧ P x
             in have Qpred : ∀ i, Q (succ i) → Q i,
                  from take i, assume Hsi : Q (succ i),
                     have H1 : succ i ≠ n,
                       from and_eliml Hsi,
                     have Psi : P (succ i),
                       from and_elimr Hsi,
                     have Pi  : P i,
                       from Ppred i Psi,
                     have neq : i ≠ n,
                       from not_intro (assume N : i = n,
                           absurd (subst Psi N) nPsn),
                     and_intro neq Pi,
                have Qm : Q m,
                  from and_intro Hne Pm,
                have nQn : ¬ Q n,
                  from not_intro
                        (assume N : n ≠ n ∧ P n,
                           absurd (refl n) (and_eliml N)),
                show m = n ∨ m < n,
                  from or_intror (m = n) (lt_intro Qpred Qm nQn)))

theorem disj_to_lt_succ {m n : num} : m = n ∨ m < n → m < succ n
:= assume H : m = n ∨ m < n,
     or_elim H
       (λ Hl : m = n,
          have H1 : n < succ n,
            from n_lt_succ_n n,
          show m < succ n,
            from substp (λ x, x < succ n) H1 (symm Hl)) -- TODO, improve elaborator to catch this case
       (λ Hr : m < n, lt_to_lt_succ Hr)

theorem lt_succ_ne_to_lt {m n : num} : m < succ n → m ≠ n → m < n
:= assume (H1 : m < succ n) (H2 : m ≠ n),
     resolve1 (lt_succ_to_disj H1) H2

definition simp_rec_rel {A : (Type U)} (fn : num → A) (x : A) (f : A → A) (n : num)
:= fn zero = x ∧ (∀ m, m < n → fn (succ m) = f (fn m))

definition simp_rec_fun {A : (Type U)} (x : A) (f : A → A) (n : num) : num → A
:= ε (inhabited_fun num (inhabited_intro x)) (λ fn : num → A, simp_rec_rel fn x f n)

-- The basic idea is:
--   (simp_rec_fun x f n) returns a function that 'works' for all m < n
-- We have that n < succ n, then we can define (simp_rec x f n) as:
definition simp_rec {A : (Type U)} (x : A) (f : A → A) (n : num) : A
:= simp_rec_fun x f (succ n) n

theorem simp_rec_def {A : (Type U)} (x : A) (f : A → A) (n : num)
                     : (∃ fn, simp_rec_rel fn x f n)
                       ↔
                       (simp_rec_fun x f n zero = x ∧
                        ∀ m, m < n → simp_rec_fun x f n (succ m) = f (simp_rec_fun x f n m))
:= iff_intro
    (assume Hl : (∃ fn, simp_rec_rel fn x f n),
       obtain (fn : num → A) (Hfn : simp_rec_rel fn x f n),
         from Hl,
       have inhab : inhabited (num → A),
         from (inhabited_fun num (inhabited_intro x)),
       show simp_rec_rel (simp_rec_fun x f n) x f n,
         from @eps_ax (num → A) inhab (λ fn, simp_rec_rel fn x f n) fn Hfn)
    (assume Hr,
       have H1 : simp_rec_rel (simp_rec_fun x f n) x f n,
         from Hr,
       show (∃ fn, simp_rec_rel fn x f n),
         from exists_intro (simp_rec_fun x f n) H1)

theorem simp_rec_ex {A : (Type U)} (x : A) (f : A → A) (n : num)
                    : ∃ fn, simp_rec_rel fn x f n
:= induction_on n
     (let fn : num → A := λ n, x in
      have fz  : fn zero = x,
        from refl (fn zero),
      have fs  : ∀ m, m < zero → fn (succ m) = f (fn m),
        from take m, assume Hmn : m < zero,
                absurd_elim (fn (succ m) = f (fn m))
                            Hmn
                            (not_lt_zero m),
      show ∃ fn, simp_rec_rel fn x f zero,
        from exists_intro fn (and_intro fz fs))
    (λ (n : num) (iH : ∃ fn, simp_rec_rel fn x f n),
       obtain (fn : num → A) (Hfn : simp_rec_rel fn x f n),
         from iH,
       let fn1 : num → A := λ m, if succ n = m then f (fn n) else fn m in
       have f1z : fn1 zero = x,
         from calc fn1 zero  = if succ n = zero then f (fn n) else fn zero  : refl (fn1 zero)
                        ...  = if false then f (fn n) else fn zero          : { eqf_intro (succ_nz n) }
                        ...  = fn zero                                      : if_false _ _
                        ...  = x                                            : and_eliml Hfn,
       have f1s : ∀ m, m < succ n → fn1 (succ m) = f (fn1 m),
         from take m, assume Hlt : m < succ n,
                 or_elim (lt_succ_to_disj Hlt)
                   (assume Hl : m = n,
                       have Hrw1  : (succ n = succ m) ↔ true,
                         from eqt_intro (symm (congr2 succ Hl)),
                       have Haux1 : (succ n = m) ↔ false,
                         from eqf_intro (subst (sn_ne_n m) Hl),
                       have Hrw2  : fn n = fn1 m,
                         from symm (calc fn1 m = if succ n = m then f (fn n) else fn m  : refl (fn1 m)
                                           ... = if false then f (fn n) else fn m       : { Haux1 }
                                           ... = fn m                                   : if_false _ _
                                           ... = fn n                                   : congr2 fn Hl),
                       calc fn1 (succ m) = if succ n = succ m then f (fn n) else fn (succ m)  : refl (fn1 (succ m))
                                 ...     = if true then f (fn n) else fn (succ m)             : { Hrw1 }
                                 ...     = f (fn n)                                           : if_true _ _
                                 ...     = f (fn1 m)                                          : { Hrw2 })
                   (assume Hr : m < n,
                       have Haux1 : fn (succ m) = f (fn m),
                         from (and_elimr Hfn m Hr),
                       have Hrw1 : (succ n = succ m) ↔ false,
                         from eqf_intro (not_intro (assume N : succ n = succ m,
                                  have nLt : ¬ m < n,
                                    from eq_to_not_lt (symm (succ_inj N)),
                                  absurd Hr nLt)),
                       have Haux2 : m < succ n,
                         from lt_to_lt_succ Hr,
                       have Haux3 : (succ n = m) ↔ false,
                         from eqf_intro (ne_symm (lt_to_neq Haux2)),
                       have Hrw2 : fn m = fn1 m,
                          from symm (calc fn1 m = if succ n = m then f (fn n) else fn m : refl (fn1 m)
                                            ... = if false then f (fn n) else fn m      : { Haux3 }
                                            ... = fn m                                  : if_false _ _),
                       calc fn1 (succ m) = if succ n = succ m then f (fn n) else fn (succ m) : refl (fn1 (succ m))
                                    ...  = if false then f (fn n) else fn (succ m)           : { Hrw1 }
                                    ...  = fn (succ m)                                       : if_false _ _
                                    ...  = f (fn m)                                          : Haux1
                                    ...  = f (fn1 m)                                         : { Hrw2 }),
       show ∃ fn, simp_rec_rel fn x f (succ n),
         from exists_intro fn1 (and_intro f1z f1s))

theorem simp_rec_lemma1 {A : (Type U)} (x : A) (f : A → A) (n : num)
                       : simp_rec_fun x f n zero = x ∧
                         ∀ m, m < n → simp_rec_fun x f n (succ m) = f (simp_rec_fun x f n m)
:= (simp_rec_def x f n) ◂ (simp_rec_ex x f n)

theorem simp_rec_lemma2 {A : (Type U)} (x : A) (f : A → A) (n m1 m2 : num)
                        : n < m1 → n < m2 → simp_rec_fun x f m1 n = simp_rec_fun x f m2 n
:= induction_on n
     (assume H1 H2,
          calc simp_rec_fun x f m1 zero = x                         : and_eliml (simp_rec_lemma1 x f m1)
                                   ...  = simp_rec_fun x f m2 zero  : symm (and_eliml (simp_rec_lemma1 x f m2)))
     (λ (n : num) (iH : n < m1 → n < m2 → simp_rec_fun x f m1 n = simp_rec_fun x f m2 n),
        assume (Hs1 : succ n < m1) (Hs2 : succ n < m2),
        have H1  : n < m1,
          from lt_succ Hs1,
        have H2  : n < m2,
          from lt_succ Hs2,
        have Heq1 : simp_rec_fun x f m1 (succ n) = f (simp_rec_fun x f m1 n),
          from and_elimr (simp_rec_lemma1 x f m1) n H1,
        have Heq2 : simp_rec_fun x f m1 n = simp_rec_fun x f m2 n,
          from iH H1 H2,
        have Heq3 : simp_rec_fun x f m2 (succ n) = f (simp_rec_fun x f m2 n),
          from and_elimr (simp_rec_lemma1 x f m2) n H2,
        calc simp_rec_fun x f m1 (succ n) = f (simp_rec_fun x f m1 n)    : Heq1
                                    ...   = f (simp_rec_fun x f m2 n)    : congr2 f Heq2
                                    ...   = simp_rec_fun x f m2 (succ n) : symm Heq3)

theorem simp_rec_thm {A : (Type U)} (x : A) (f : A → A)
                     : simp_rec x f zero = x ∧
                       ∀ m, simp_rec x f (succ m) = f (simp_rec x f m)
:= have Heqz : simp_rec_fun x f (succ zero) zero = x,
     from and_eliml (simp_rec_lemma1 x f (succ zero)),
   have Hz : simp_rec x f zero = x,
     from calc simp_rec x f zero = simp_rec_fun x f (succ zero) zero : refl _
                           ...   = x                                 : Heqz,
   have Hs : ∀ m, simp_rec x f (succ m) = f (simp_rec x f m),
     from take m,
       have Hlt1 : m < succ (succ m),
         from lt_to_lt_succ (n_lt_succ_n m),
       have Hlt2 : m < succ m,
         from n_lt_succ_n m,
       have Heq1 : simp_rec_fun x f (succ (succ m)) (succ m) = f (simp_rec_fun x f (succ (succ m)) m),
         from and_elimr (simp_rec_lemma1 x f (succ (succ m))) m Hlt1,
       have Heq2 : simp_rec_fun x f (succ (succ m)) m = simp_rec_fun x f (succ m) m,
         from simp_rec_lemma2 x f m (succ (succ m)) (succ m) Hlt1 Hlt2,
       calc simp_rec x f (succ m) = simp_rec_fun x f (succ (succ m)) (succ m) : refl _
                           ...    = f (simp_rec_fun x f (succ (succ m)) m)    : Heq1
                           ...    = f (simp_rec_fun x f (succ m) m)           : { Heq2 }
                           ...    = f (simp_rec x f m)                        : refl _,
   show simp_rec x f zero = x ∧ ∀ m, simp_rec x f (succ m) = f (simp_rec x f m),
     from and_intro Hz Hs

definition pre (m : num) := if m = zero then zero else ε inhab (λ n, succ n = m)

set_option simplifier::unfold true

theorem pre_zero : pre zero = zero
:= by simp

theorem pre_succ (m : num) : pre (succ m) = m
:= have Heq : (λ n, succ n = succ m) = (λ n, n = m),
     from funext (λ n, iff_intro (assume Hl, succ_inj Hl)
                                 (assume Hr, congr2 succ Hr)),
   calc pre (succ m) = ε inhab (λ n, succ n = succ m)  : by simp
                 ... = ε inhab (λ n, n = m)            : { Heq }
                 ... = m                               : eps_singleton inhab m

definition prim_rec_fun {A : (Type U)} (x : A) (f : A → num → A)
:= simp_rec (λ n : num, x) (λ fn n, f (fn (pre n)) n)

definition prim_rec {A : (Type U)} (x : A) (f : A → num → A) (m : num)
:= prim_rec_fun x f m (pre m)

theorem prim_rec_thm {A : (Type U)} (x : A) (f : A → num → A)
                     : prim_rec x f zero = x ∧
                       ∀ m, prim_rec x f (succ m) = f (prim_rec x f m) m
:= let faux := λ fn n, f (fn (pre n)) n in
   have Hz : prim_rec x f zero = x,
     from have Heq1 : simp_rec (λ n, x) faux zero = (λ n : num, x),
            from and_eliml (simp_rec_thm (λ n, x) faux),
          calc prim_rec x f zero = prim_rec_fun x f zero (pre zero)       : refl _
                           ...   = prim_rec_fun x f zero zero             : { pre_zero }
                           ...   = simp_rec (λ n, x) faux zero zero       : refl _
                           ...   = x                                      : congr1 zero Heq1,
   have Hs : ∀ m, prim_rec x f (succ m) = f (prim_rec x f m) m,
     from take m,
        have Heq1 : pre (succ m) = m,
          from pre_succ m,
        have Heq2 : simp_rec (λ n, x) faux (succ m) = faux (simp_rec (λ n, x) faux m),
          from and_elimr (simp_rec_thm (λ n, x) faux) m,
        calc prim_rec x f (succ m) = prim_rec_fun x f (succ m) (pre (succ m)) : refl _
                              ...  = prim_rec_fun x f (succ m) m              : congr2 (prim_rec_fun x f (succ m)) Heq1
                              ...  = simp_rec (λ n, x) faux (succ m) m        : refl _
                              ...  = faux (simp_rec (λ n, x) faux m) m        : congr1 m Heq2
                              ...  = f (prim_rec x f m) m                     : refl _,
   show prim_rec x f zero = x ∧ ∀ m, prim_rec x f (succ m) = f (prim_rec x f m) m,
     from and_intro Hz Hs

set_opaque simp_rec_rel true
set_opaque simp_rec_fun true
set_opaque simp_rec     true
set_opaque prim_rec_fun true
set_opaque prim_rec     true
end

definition num := num::num