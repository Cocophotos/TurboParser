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

#ifndef FACTOR_ARGUMENT_AUTOMATON
#define FACTOR_ARGUMENT_AUTOMATON

#include "SemanticPart.h"
#include "ad3/GenericFactor.h"
#ifdef _WIN32
#include <unordered_map>
#else
#include <tr1/unordered_map>
#endif

namespace AD3 {

typedef std::tr1::unordered_map<int, int> HashMapIntInt;

class FactorArgumentAutomaton : public GenericFactor {
 public:
  FactorArgumentAutomaton() {}
  virtual ~FactorArgumentAutomaton() { ClearActiveSet(); }

  // Print as a string.
  void Print(ostream& stream) {
    stream << "ARGUMENT_AUTOMATON";
    Factor::Print(stream);
#if 0
    // Print number of senses.
    stream << " " << GetNumSenses();
    int total = 0; // Delete this later.
    for (int s = 0; s < GetNumSenses(); ++s) {
      for (int a1 = 0; a1 < index_siblings_[s].size(); ++a1) {
        for (int a2 = a1+1; a2 <= index_siblings_[s][a1].size(); ++a2) {
          CHECK_GE(index_siblings_[s][a1][a2], 0);
          int index = index_siblings_[s][a1][a2];
          ++total;
          stream << " " << setprecision(9)
                 << additional_log_potentials_[index];
        }
      }
    }
    stream << endl;
    CHECK_EQ(additional_log_potentials_.size(), total);
#endif
  }

  int GetLength() const { return index_arcs_.size(); }
  int GetNumSenses(int p) const {
    if (p == 0 || p == GetLength()) {
      return 1;
    } else {
      return index_arcs_[p].size();
    }
  }
  double GetPredicateScore(
      int predicate,
      int sense,
      const vector<double> &variable_log_potentials,
      const vector<double> &additional_log_potentials) const {
    int index_sense = index_senses_[predicate][sense];
    int index = index_arcs_[predicate][sense];
    return variable_log_potentials[index_sense] +
      variable_log_potentials[index];
  }
  double GetCoparentScore(
      int first_predicate,
      int first_sense,
      int second_predicate,
      int second_sense,
      const vector<double> &variable_log_potentials,
      const vector<double> &additional_log_potentials) const {
    int index = index_coparents_[first_predicate][first_sense]
      [second_predicate][second_sense];
    return additional_log_potentials[index];
  }
  void AddPredicatePosterior(int predicate,
                             int sense,
                             double weight,
                             vector<double> *variable_posteriors,
                             vector<double> *additional_posteriors) const {
    int index_sense = index_senses_[predicate][sense];
    int index = index_arcs_[predicate][sense];
    (*variable_posteriors)[index_sense] += weight;
    (*variable_posteriors)[index] += weight;
  }
  void AddCoparentPosterior(int first_predicate,
                            int first_sense,
                            int second_predicate,
                            int second_sense,
                            double weight,
                            vector<double> *variable_posteriors,
                            vector<double> *additional_posteriors) const {
    int index = index_coparents_[first_predicate][first_sense]
      [second_predicate][second_sense];
    (*additional_posteriors)[index] += weight;
  }

  // Compute the score of a given assignment.
  void Maximize(const vector<double> &variable_log_potentials,
                const vector<double> &additional_log_potentials,
                Configuration &configuration,
                double *value) {
#if 0
    // Decode maximizing over the senses and using the Viterbi algorithm
    // as an inner loop.
    // If sense=-1, the final score is zero (so take the argmax at the end).
    int num_senses = GetNumSenses();
    int best_sense = -1;
    vector<int> best_path;
    *value = 0.0;

    // Run Viterbi for each possible grandparent.
    for (int s = 0; s < num_senses; ++s) {
      int length = GetLength(s);
      vector<vector<double> > values(length);
      vector<vector<int> > path(length);
      CHECK_GE(length, 1);

      // The start state is a1 = 0.
      values[0].resize(1);
      values[0][0] = 0.0;
      path[0].resize(1);
      path[0][0] = 0;
      for (int a = 1; a < length; ++a) {
        // a+1 possible states: either keep the previous state (no arc added)
        // or transition to a new state (arc between p and a).
        values[a].resize(a+1);
        path[a].resize(a+1);
        for (int i = 0; i < a; ++i) {
          // In this case, the previous state must also be i.
          values[a][i] = values[a-1][i];
          path[a][i] = i;
        }
        // For the a-th state, the previous state can be anything up to a-1.
        path[a][a] = -1;
        for (int j = 0; j < a; ++j) {
          double score = values[a-1][j];
          score += GetSiblingScore(s, j, a, variable_log_potentials,
                                   additional_log_potentials);
          if (path[a][a] < 0 || score > values[a][a]) {
            values[a][a] = score;
            path[a][a] = j;
          }
        }
        values[a][a] += GetArgumentScore(s, a, variable_log_potentials,
                                         additional_log_potentials);
      }

      // The end state is a = length.
      int best_last_state = -1;
      double best_score = -1e12;
      for (int j = 0; j < length; ++j) {
        int index = index_siblings_[s][j][length];
        CHECK_GE(index, 0);
        double score = values[length-1][j] +
          GetSiblingScore(s, j, length, variable_log_potentials,
                          additional_log_potentials);
        if (best_last_state < 0 || score > best_score) {
          best_score = score;
          best_last_state = j;
        }
      }

      // Add the score of the predicate sense s.
      // Note: we're allowing senses != -1 with no argument frame.
      best_score += GetSenseScore(s, variable_log_potentials,
                                  additional_log_potentials);

      // Only backtrack if the solution is the best so far.
      if (best_score > *value) {
        // This is the best sense so far.
        best_sense = s;
        *value = best_score;
        best_path.resize(length);
        best_path[length-1] = best_last_state;

        // Backtrack.
        for (int a = length-1; a > 0; --a) {
          best_path[a-1] = path[a][best_path[a]];
        }
      }
    }

    // Now write the configuration.
    vector<int> *sense_arguments =
      static_cast<vector<int>*>(configuration);
    sense_arguments->push_back(best_sense);
    int length = (best_sense >= 0)? GetLength(best_sense) : 0;
    for (int a = 1; a < length; ++a) {
      if (best_path[a] == a) {
        sense_arguments->push_back(a);
      }
    }
#endif
  }

  // Compute the score of a given assignment.
  void Evaluate(const vector<double> &variable_log_potentials,
                const vector<double> &additional_log_potentials,
                const Configuration configuration,
                double *value) {
    const vector<int> *predicates_senses =
      static_cast<const vector<int>*>(configuration);
    *value = 0.0;
    // Predicates belong to {1,2,...}
    // Senses belong to {0,1,...}
    CHECK_EQ(predicates_senses->size() % 2, 0);
    int num_predicates = predicates_senses->size() / 2;
    int p1 = 0; // Start position.
    int s1 = 0; // First predicate sense is 0 by convention.
    for (int i = 0; i < num_predicates; ++i) {
      int p2 = (*predicates_senses)[i];
      int s2 = (*predicates_senses)[num_predicates + i];
      *value += GetPredicateScore(p2, s2, variable_log_potentials,
                                  additional_log_potentials);
      *value += GetCoparentScore(p1, s1, p2, s2, variable_log_potentials,
                                 additional_log_potentials);
      p1 = p2;
      s1 = s2;
    }
    int p2 = GetLength(); // Stop position.
    int s2 = 0; // Last predicate sense is 0 by convention.
    *value += GetCoparentScore(p1, s1, p2, s2, variable_log_potentials,
                               additional_log_potentials);
  }

  // Given a configuration with a probability (weight),
  // increment the vectors of variable and additional posteriors.
  void UpdateMarginalsFromConfiguration(
    const Configuration &configuration,
    double weight,
    vector<double> *variable_posteriors,
    vector<double> *additional_posteriors) {
    const vector<int> *predicates_senses =
      static_cast<const vector<int>*>(configuration);
    // Predicates belong to {1,2,...}
    // Senses belong to {0,1,...}
    CHECK_EQ(predicates_senses->size() % 2, 0);
    int num_predicates = predicates_senses->size() / 2;
    int p1 = 0; // Start position.
    int s1 = 0; // First predicate sense is 0 by convention.
    for (int i = 0; i < num_predicates; ++i) {
      int p2 = (*predicates_senses)[i];
      int s2 = (*predicates_senses)[num_predicates + i];
      AddPredicatePosterior(p2, s2, weight, variable_posteriors,
                            additional_posteriors);
      AddCoparentPosterior(p1, s1, p2, s2, weight, variable_posteriors,
                           additional_posteriors);
      p1 = p2;
      s1 = s2;
    }
    int p2 = GetLength(); // Stop position.
    int s2 = 0; // Last predicate sense is 0 by convention.
    AddCoparentPosterior(p1, s1, p2, s2, weight, variable_posteriors,
                         additional_posteriors);
  }

  // Count how many common values two configurations have.
  int CountCommonValues(const Configuration &configuration1,
                        const Configuration &configuration2) {
    const vector<int> *values1 = static_cast<const vector<int>*>(configuration1);
    const vector<int> *values2 = static_cast<const vector<int>*>(configuration2);
    CHECK_EQ(values1->size() % 2, 0);
    CHECK_EQ(values2->size() % 2, 0);
    int length1 = values1->size() / 2;
    int length2 = values2->size() / 2;
    int count = 0;
    int j = 0;
    for (int i = 0; i < length1; ++i) {
      for (; j < length2; ++j) {
        if ((*values2)[j] >= (*values1)[i]) break;
      }
      if (j < values2->size() && (*values2)[j] == (*values1)[i]) {
        // Matched predicate index; check predicate sense.
        int sense1 = (*values1)[length1 + i];
        int sense2 = (*values2)[length2 + j];
        if (sense1 == sense2) count += 2; // Matched arc and predicate sense.
        ++j;
      }
    }
    return count;
  }

  // Check if two configurations are the same.
  bool SameConfiguration(
    const Configuration &configuration1,
    const Configuration &configuration2) {
    const vector<int> *values1 = static_cast<const vector<int>*>(configuration1);
    const vector<int> *values2 = static_cast<const vector<int>*>(configuration2);
    if (values1->size() != values2->size()) return false;
    for (int i = 0; i < values1->size(); ++i) {
      if ((*values1)[i] != (*values2)[i]) return false;
    }
    return true;
  }

  // Delete configuration.
  void DeleteConfiguration(
    Configuration configuration) {
    vector<int> *values = static_cast<vector<int>*>(configuration);
    delete values;
  }

  Configuration CreateConfiguration() {
    // The first half of this array contains the indices of the predicates.
    // The second half contains the predicate senses.
    vector<int>* predicates_senses = new vector<int>;
    return static_cast<Configuration>(predicates_senses);
  }

 public:
  // Predicate senses are of the form (p,s) for each p and s.
  // Incoming arcs are of the form (p,s,a) for each p and s.
  // The variables linked to this factor must be in the same order as
  // the predicate senses, followed by the incoming arcs.
  // "Right" indicates the attachment direction (if true, then p <= a1 < a2).
  void Initialize(int argument,
                  bool right,
                  const vector<SemanticPartPredicate*> &predicate_senses,
                  const vector<SemanticPartArc*> &incoming_arcs,
                  const vector<SemanticPartConsecutiveCoparent*> &coparents) {
    // Get argument index.
    int a = argument;

    // Build map of senses.
    vector<HashMapIntInt> map_senses;
    int num_senses = predicate_senses.size();
    CHECK_GT(num_senses, 0);
    for (int k = 0; k < predicate_senses.size(); ++k) {
      int p = predicate_senses[k]->predicate();
      int s = predicate_senses[k]->sense();
      int position = (right)? p-a : a-p;
      ++position; // Position 0 is reserved for the case a1=-1.
      CHECK_GE(position, 1);
      if (position >= map_senses.size()) {
        map_senses.resize(position+1);
      }
      CHECK(map_senses[position].find(s) == map_senses[position].end());
      // This will be replaced by the index of the sense.
      map_senses[position][s] = k;
    }

    index_senses_.clear();
    // Always put something in the zero position (which is special).
    // In this case, there are no senses.
    index_senses_.push_back(vector<int>(0));
    for (int position = 1; position < map_senses.size(); ++position) {
      int num_senses = map_senses[position].size();
      int trimmed_position = index_senses_.size();
      if (num_senses == 0) continue;
      index_senses_.push_back(vector<int>(num_senses, -1));
      int sense = 0;
      for (HashMapIntInt::iterator it = map_senses[position].begin();
           it != map_senses[position].end(); ++it) {
        int index = it->second;
        CHECK_GE(index, 0);
        // Replace map_senses[position][sense] by the index of the sense.
        map_senses[position][it->first] = sense;
        // Index of outgoing arc in the variables array.
        index_senses_[trimmed_position][sense] = index;
        ++sense;
      }
    }

    // Create a temporary list of arguments.
    // Each argument position will be mapped to a one-based array, in
    // which sense_arguments[s][1] = p, sense_arguments[s][2] = p+1 (p-1),
    // etc. for the right (left) automaton case. Self-loops are possible.
    // The zero position is reserved to denote a a1 = -1 (in a2 will be the
    // first argument of the predicate).
    // E.g. for a right automaton with p=3, the argument a1=7 will be stored in
    // position 7-3+1=5.
    int offset_incoming_arcs = num_senses;
    vector<vector<int> > predicates(1, vector<int>(0));
    for (int k = 0; k < incoming_arcs.size(); ++k) {
      CHECK_EQ(a, incoming_arcs[k]->argument());
      int p = incoming_arcs[k]->predicate();
      int s = incoming_arcs[k]->sense();
      int position = (right)? p-a : a-p;
      ++position; // Position 0 is reserved for the case a1=-1.
      CHECK_GE(position, 1) << p << " " << a;
      CHECK(map_senses[position].find(s) != map_senses[position].end());
      int sense = map_senses[position][s];
      if (position >= predicates.size()) {
        predicates.resize(position+1);
      }
      if (sense >= predicates[position].size()) {
        predicates[position].resize(sense+1, -1);
      }
      // This will be replaced by the index of the argument.
      CHECK_LT(predicates[position][sense], 0);
      predicates[position][sense] = offset_incoming_arcs + k;
    }

    // TODO: need to make index_senses_ too!!!
    index_arcs_.clear();
    // Always put something in the zero position (which is special).
    // In this case, there are no senses.
    index_arcs_.push_back(vector<int>(0));
    for (int position = 1; position < predicates.size(); ++position) {
      int num_senses = predicates[position].size();
      int trimmed_position = index_arcs_.size();
      if (num_senses == 0) continue;
      index_arcs_.push_back(vector<int>(num_senses, -1));
      for (int sense = 0; sense < num_senses; ++sense) {
        int index = predicates[position][sense];
        if (index >= 0) {
          // Replace predicates[sense][position] by the index of the arc.
          predicates[position][sense] = trimmed_position;
          // Index of outgoing arc in the variables array.
          index_arcs_[trimmed_position][sense] = index;
        }
      }
    }

    // Construct index of siblings.
    int num_predicates = GetLength();
    index_coparents_.assign(num_predicates, vector<vector<vector<int> > >(0));
    for (int i = 0; i < num_predicates; ++i) {
      // Pretend there is a sense 0 for the first predicate.
      int num_senses1 = GetNumSenses(i); //(i == 0)? 1 : index_arcs_[i].size();
      index_coparents_[i].assign(num_senses1,
                                 vector<vector<int> >(num_predicates+1));
      for (int sense1 = 0; sense1 < num_senses1; ++sense1) {
        for (int j = 0; j < num_predicates+1; ++j) {
          // Pretend there is a sense 0 for the last predicate.
          int num_senses2 = GetNumSenses(j); // (j == num_predicates)? 1 : index_arcs_[j].size();
          index_coparents_[i][sense1][j].assign(num_senses2, -1);
        }
      }
    }
    for (int k = 0; k < coparents.size(); ++k) {
      CHECK_EQ(a, coparents[k]->argument());
      int p1 = coparents[k]->first_predicate();
      int s1 = coparents[k]->first_sense();
      int p2 = coparents[k]->second_predicate();
      int s2 = coparents[k]->second_sense();
      int position1 = right? p1-a : a-p1;
      int position2 = right? p2-a : a-p2;
      if (p1 < 0) position1 = -1; // To handle p1=-1.
      CHECK(map_senses[p1].find(s1) != map_senses[p1].end());
      int sense1 = map_senses[p1][s1];
      CHECK(map_senses[p2].find(s2) != map_senses[p2].end());
      int sense2 = map_senses[p2][s2];
      ++position1; // Position 0 is reserved for the case p1=-1.
      ++position2;
      CHECK_GE(position1, 0);
      CHECK_GE(position2, 1);
      CHECK_LT(position1, predicates.size());
      CHECK_LT(position2, predicates.size()+1); // Was commented?
      int first_predicate = (position1 > 0)?
        predicates[position1][sense1] : 0;
      int second_predicate = (position2 < predicates.size())?
        predicates[position2][sense2] : GetLength();
      CHECK_GE(first_predicate, 0);
      CHECK_GE(second_predicate, 1);
      CHECK_LT(first_predicate, index_coparents_.size());
      CHECK_LT(second_predicate,
               index_coparents_[first_predicate][sense1].size());
      // Index of co-parents in the additional_variables array.
      index_coparents_[first_predicate][sense1][second_predicate][sense2] = k;
    }
  }

 private:
  vector<vector<int> > index_senses_; // Indexed by p, s.
  vector<vector<int> > index_arcs_; // Indexed by p, s.
  // Indexed by p1, s1, p2, s2.
  vector<vector<vector<vector<int> > > > index_coparents_;
};

} // namespace AD3

#endif // FACTOR_ARGUMENT_AUTOMATON
