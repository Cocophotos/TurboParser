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

// Special symbols.
const string kPredicateUnknown = "_UNKNOWN_.01"; // Unknown predicate.

// Maximum alphabet sizes.
const unsigned int kMaxPredicateAlphabetSize = 0xffff;
const unsigned int kMaxRoleAlphabetSize = 0xffff;

void SemanticDictionary::CreatePredicateRoleDictionaries(SemanticReader *reader) {
  LOG(INFO) << "Creating predicate and role dictionaries...";

  // Initialize lemma predicates.
  int num_lemmas = token_dictionary_->GetNumLemmas();
  lemma_predicates_.resize(num_lemmas);

  vector<int> role_freqs;
  vector<int> predicate_freqs;

  string special_symbols[NUM_SPECIAL_PREDICATES];
  special_symbols[PREDICATE_UNKNOWN] = kPredicateUnknown;

  for (int i = 0; i < NUM_SPECIAL_PREDICATES; ++i) {
    predicate_alphabet_.Insert(special_symbols[i]);

    // Counts of special symbols are set to -1:
    predicate_freqs.push_back(-1);
  }

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

      // If the lemma does not exist, the predicate will not be added.
      SemanticPredicate *predicate = NULL;
      if (lemma_id >= 0) {
        // Add predicate name to alphabet.
        int predicate_id =
          predicate_alphabet_.Insert(predicate_name);
        if (predicate_id >= predicate_freqs.size()) {
          CHECK_EQ(predicate_id, predicate_freqs.size());
          predicate_freqs.push_back(0);
        }
        ++predicate_freqs[predicate_id];

        // Add predicate to the list of lemma predicates.
        std::vector<SemanticPredicate*> *predicates =
          &lemma_predicates_[lemma_id];
        for (int j = 0; j < predicates->size(); ++j) {
          if ((*predicates)[j]->id() == predicate_id) {
            predicate = (*predicates)[j];
          }
        }
        if (!predicate) {
          predicate = new SemanticPredicate(predicate_id);
          predicates->push_back(predicate);
        }
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
        if (predicate && !predicate->HasRole(role_id)) {
          predicate->InsertRole(role_id);
        }
      }
    }
    delete instance;
    instance = static_cast<SemanticInstance*>(reader->GetNext());
  }
  reader->Close();
  role_alphabet_.StopGrowth();

  // Take care of the special "unknown" predicate.
  bool allow_unseen_predicates =
    static_cast<SemanticPipe*>(pipe_)->GetSemanticOptions()->
       allow_unseen_predicates();
  if (allow_unseen_predicates) {
    // 1) Add the predicate as the singleton list of lemma predicates for the
    // "unknown" lemma.
    std::vector<SemanticPredicate*> *predicates =
      &lemma_predicates_[TOKEN_UNKNOWN];
    CHECK_EQ(predicates->size(), 0);
    SemanticPredicate *predicate = new SemanticPredicate(PREDICATE_UNKNOWN);
    predicates->push_back(predicate);

    // 2) Add all possible roles to the special "unknown" predicate.
    for (int role_id = 0; role_id < role_alphabet_.size(); ++role_id) {
      if (!predicate->HasRole(role_id)) predicate->InsertRole(role_id);
    }
  }

  predicate_alphabet_.StopGrowth();

  CHECK_LT(predicate_alphabet_.size(), kMaxPredicateAlphabetSize);
  CHECK_LT(role_alphabet_.size(), kMaxRoleAlphabetSize);

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

        // Compute the syntactic path between the predicate and the argument and
        // add it to the dictionary.
        string relation_path;
        string pos_path;
        ComputeDependencyPath(instance, p, a, &relation_path, &pos_path);
        int relation_path_id = relation_path_alphabet_.Insert(relation_path);
        int pos_path_id = pos_path_alphabet_.Insert(pos_path);
      }
    }
    delete instance;
    instance = static_cast<SemanticInstance*>(reader->GetNext());
  }
  reader->Close();
  relation_path_alphabet_.StopGrowth();
  pos_path_alphabet_.StopGrowth();

  CHECK_LT(relation_path_alphabet_.size(), 0xffff);
  CHECK_LT(pos_path_alphabet_.size(), 0xffff);

  LOG(INFO) << "Number of predicates: " << predicate_alphabet_.size();
  LOG(INFO) << "Number of roles: " << role_alphabet_.size();
  LOG(INFO) << "Number of relation paths: " << relation_path_alphabet_.size();
  LOG(INFO) << "Number of POS paths: " << pos_path_alphabet_.size();
}

void SemanticDictionary::ComputeDependencyPath(SemanticInstance *instance,
                                               int p, int a,
                                               string *relation_path,
                                               string *pos_path) const {
  const vector<int>& heads = instance->GetHeads();
  vector<string> relations_up;
  vector<string> relations_down;
  vector<string> pos_up;
  vector<string> pos_down;

  int ancestor = FindLowestCommonAncestor(heads, p, a);
  int h = p;
  while (ancestor != h) {
    relations_up.push_back(instance->GetDependencyRelation(h));
    pos_up.push_back(instance->GetPosTag(h));
    h = heads[h];
  }
  h = a;
  while (ancestor != h) {
    relations_down.push_back(instance->GetDependencyRelation(h));
    pos_down.push_back(instance->GetPosTag(h));
    h = heads[h];
  }

  relation_path->clear();
  pos_path->clear();
  for (int i = 0; i < relations_up.size(); ++i) {
    *relation_path += relations_up[i] + "^";
    *pos_path += pos_up[i] + "^";
  }
  *pos_path += instance->GetPosTag(ancestor);
  for (int i = relations_down.size()-1; i >= 0; --i) {
    *relation_path += relations_down[i] + "!";
    *pos_path += pos_down[i] + "!";
  }
}

int SemanticDictionary::FindLowestCommonAncestor(const vector<int>& heads,
                                                 int p, int a) const {
  vector<bool> is_ancestor(heads.size(), false);
  int h = p;
  // 0 is the root and is a common ancestor.
  while (h != 0) {
    is_ancestor[h] = true;
    h = heads[h];
  }
  h = a;
  while (h != 0) {
    if (is_ancestor[h]) return h;
    h = heads[h];
  }
  return 0;
}
