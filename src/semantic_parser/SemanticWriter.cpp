// Copyright (c) 2012-2013 Andre Martins
// All Rights Reserved.
//
// This file is part of TurboParser 2.1.
//
// TurboParser 2.1 is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// TurboParser 2.1 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with TurboParser 2.1.  If not, see <http://www.gnu.org/licenses/>.

#include "SemanticWriter.h"
#include "SemanticInstance.h"
#include <iostream>
#include <sstream>

void SemanticWriter::Write(Instance *instance) {
  SemanticInstance *semantic_instance =
    static_cast<SemanticInstance*>(instance);

  bool write_semantic_roles = true;
  int current_predicate = 0;

  for (int i = 1; i < semantic_instance->size(); ++i) {
    os_ << i << "\t";
    os_ << "_" << "\t"; // Change this later
    os_ << "_" << "\t"; // Change this later
    os_ << "_" << "\t"; // Change this later
    os_ << "_" << "\t"; // Change this later
    os_ << semantic_instance->GetForm(i) << "\t";
    os_ << semantic_instance->GetLemma(i) << "\t";
    os_ << semantic_instance->GetPosTag(i) << "\t";
    os_ << semantic_instance->GetHead(i) << "\t";
    os_ << semantic_instance->GetDependencyRelation(i) << "\t";

    if (write_semantic_roles) {
      string predicate_name = "_";
      if (current_predicate < semantic_instance->GetNumPredicates() &&
          i == semantic_instance->GetPredicateIndex(current_predicate)) {
        predicate_name = semantic_instance->GetPredicateName(current_predicate);
        ++current_predicate;
      }
      os_ << predicate_name << "\t";
      for (int k = 0; k < semantic_instance->GetNumPredicates(); ++k) {
        string argument_name = "_";
        for (int l = 0; l < semantic_instance->GetNumArgumentsPredicate(k); ++l) {
          if (i == semantic_instance->GetArgumentIndex(k, l)) {
            argument_name = semantic_instance->GetArgumentRole(k, l);
          }
        }
        os_ << argument_name << "\t";
      }
    }

    os_ << endl;
  }
  os_ << endl;
}
