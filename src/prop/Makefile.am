AUTOMAKE_OPTIONS = subdir-objects
noinst_LTLIBRARIES = libbagel_prop.la
libbagel_prop_la_SOURCES = multipole.cc hyperfine.cc momentum.cc momentum_london.cc momentum_point.cc current.cc overlap_point.cc moprint.cc sphmultipole.cc \
pseudospin/stevensop.cc pseudospin/pseudospin.cc \
\
proptool/proptool.cc proptool/proputils.cc \
\
proptool/algebraic_manipulator/equation.cc proptool/algebraic_manipulator/equation_linearRM.cc \
proptool/algebraic_manipulator/braket.cc proptool/algebraic_manipulator/tensop.cc \
proptool/algebraic_manipulator/ctrtensop.cc proptool/algebraic_manipulator/expression.cc \
proptool/algebraic_manipulator/expression_full.cc proptool/algebraic_manipulator/expression_orb_exc_deriv.cc \
proptool/algebraic_manipulator/gamma_info.cc proptool/algebraic_manipulator/gamma_generator_base.cc \
proptool/algebraic_manipulator/braket_full.cc proptool/algebraic_manipulator/gamma_generator_redux.cc \
proptool/algebraic_manipulator/braket_orb_exc_deriv.cc proptool/algebraic_manipulator/gamma_generator_orb_exc_deriv.cc \
proptool/algebraic_manipulator/symmetry_operations.cc proptool/algebraic_manipulator/system_info.cc proptool/algebraic_manipulator/range_block_info.cc \
proptool/algebraic_manipulator/constraints.cc proptool/algebraic_manipulator/op_info.cc \
proptool/algebraic_manipulator/rdm_tester.cc \
\
proptool/initialization/equation_init.cc proptool/initialization/equation_init_linearRM.cc \
\
proptool/tensor_and_ci_lib/b_gamma_computer.cc proptool/tensor_and_ci_lib/tensor_arithmetic.cc proptool/tensor_and_ci_lib/tensor_arithmetic_utils.cc \
proptool/tensor_and_ci_lib/gamma_computer.cc proptool/tensor_and_ci_lib/vector_bundle.cc proptool/tensor_and_ci_lib/ci_type_converter.cc \
\
proptool/task_translator/tensop_computer.cc proptool/task_translator/expression_computer.cc \
proptool/task_translator/equation_computer.cc proptool/task_translator/system_computer.cc \
proptool/task_translator/equation_computer_linearRM.cc \
\
proptool/integrals/moint.cc proptool/integrals/moint_computer.cc \
proptool/integrals/moint_init.cc proptool/tensor_and_ci_lib/tensor_sorter.cc

AM_CXXFLAGS=-I$(top_srcdir)
