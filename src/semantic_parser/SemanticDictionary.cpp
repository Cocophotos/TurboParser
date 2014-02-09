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

#include "SemanticDictionary.h"
#include "SemanticPipe.h"

void SemanticDictionary::CreatePredicateRoleDictionaries(SemanticReader *reader) {
  LOG(INFO) << "Creating predicate and role dictionaries...";

  // Initialize lemma predicates.
  int num_lemmas = token_dictionary_->GetNumLemmas();
  lemma_predicates_.resize(num_lemmas);

  vector<int> role_freqs;

  // Go through the corpus and build the predicate/roles dictionaries,
  // counting the frequencies.
  reader->Open(pipe_->GetOptions()->GetTrainingFilePath());
  SemanticInstance *instance =
    static_cast<SemanticInstance*>(reader->GetNext());
  int instance_length = instance->size();
  while (instance != NULL) {
    for (int k = 0; k < instance->GetNumPredicates(); ++k) {
      int i = instance->GetPredicateIndex(k);
      const std::string lemma = instance->GetLemma(i);
      const std::string predicate_name = instance->GetPredicateName(k);

      // Get the lemma integer representation.
      int lemma_id = token_dictionary_->GetLemmaId(lemma);
      // Assume lemma exists.
      // TODO(atm): handle the case where there is a label cutoff.
      CHECK_GE(lemma_id, 0);

      // Add predicate name to alphabet.
      int predicate_id =
        predicate_alphabet_.Insert(predicate_name);

      // Add predicate to the list of lemma predicates.
      std::vector<SemanticPredicate*> *predicates =
        &lemma_predicates_[lemma_id];
      SemanticPredicate *predicate = NULL;
      for (int j = 0; j < predicates->size(); ++j) {
        if ((*predicates)[j]->id() == predicate_id) {
          predicate = (*predicates)[j];
        }
      }
      if (!predicate) {
        predicate = new SemanticPredicate(predicate_id);
        predicates->push_back(predicate);
      }

      // Add semantic roles to alphabet.
      for (int l = 0; l < instance->GetNumArgumentsPredicate(k); ++l) {
        int role_id = role_alphabet_.Insert(instance->GetArgumentRole(k, l));
        if (role_id >= role_freqs.size()) {
          CHECK_EQ(role_id, role_freqs.size());
          role_freqs.push_back(0);
        }
        ++role_freqs[role_id];
        // Add this role to the predicate.
        if (!predicate->HasRole(role_id)) predicate->InsertRole(role_id);
      }
    }
    delete instance;
    instance = static_cast<SemanticInstance*>(reader->GetNext());
  }
  reader->Close();
  role_alphabet_.StopGrowth();

  // Go through the corpus and build the existing labels for each head-modifier
  // POS pair.
  existing_roles_.clear();
  existing_roles_.resize(token_dictionary_->GetNumPosTags(),
                         vector<vector<int> >(
                           token_dictionary_->GetNumPosTags()));

  maximum_left_distances_.clear();
  maximum_left_distances_.resize(token_dictionary_->GetNumPosTags(),
                                 vector<int>(
                                   token_dictionary_->GetNumPosTags(), 0));

  maximum_right_distances_.clear();
  maximum_right_distances_.resize(token_dictionary_->GetNumPosTags(),
                                  vector<int>(
                                    token_dictionary_->GetNumPosTags(), 0));

  reader->Open(pipe_->GetOptions()->GetTrainingFilePath());
  instance = static_cast<SemanticInstance*>(reader->GetNext());
  while (instance != NULL) {
    int instance_length = instance->size();
    for (int k = 0; k < instance->GetNumPredicates(); ++k) {
      int p = instance->GetPredicateIndex(k);
      const string &predicate_pos = instance->GetPosTag(p);
      int predicate_pos_id = token_dictionary_->GetPosTagId(predicate_pos);
      if (predicate_pos_id < 0) predicate_pos_id = TOKEN_UNKNOWN;

      // Add semantic roles to alphabet.
      for (int l = 0; l < instance->GetNumArgumentsPredicate(k); ++l) {
        int a = instance->GetArgumentIndex(k, l);
        const string &argument_pos = instance->GetPosTag(a);
        int argument_pos_id = token_dictionary_->GetPosTagId(argument_pos);
        if (argument_pos_id < 0) argument_pos_id = TOKEN_UNKNOWN;
        int id = role_alphabet_.Lookup(instance->GetArgumentRole(k, l));
        CHECK_GE(id, 0);

        // Insert new role in the set of existing labels, if it is not there
        // already. NOTE: this is inefficient, maybe we should be using a
        // different data structure.
        vector<int> &roles = existing_roles_[predicate_pos_id][argument_pos_id];
        int j;
        for (j = 0; j < roles.size(); ++j) {
          if (roles[j] == id) break;
        }
        if (j == roles.size()) roles.push_back(id);

        // Update the maximum distances if necessary.
        if (p < a) {
          // Right attachment.
          if (a - p >
              maximum_right_distances_[predicate_pos_id][argument_pos_id]) {
            maximum_right_distances_[predicate_pos_id][argument_pos_id] = a - p;
          }
        } else {
          // Left attachment (or self-loop). TODO(atm): treat self-loops differently?
          if (p - a >
              maximum_left_distances_[predicate_pos_id][argument_pos_id]) {
            maximum_left_distances_[predicate_pos_id][argument_pos_id] = p - a;
          }
        }
      }
    }
    delete instance;
    instance = static_cast<SemanticInstance*>(reader->GetNext());
  }
  reader->Close();

  LOG(INFO) << "Number of predicates: " << predicate_alphabet_.size();
  LOG(INFO) << "Number of roles: " << role_alphabet_.size();
}
