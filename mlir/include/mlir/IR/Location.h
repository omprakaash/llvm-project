//===- Location.h - MLIR Location Classes -----------------------*- C++ -*-===//
//
// Copyright 2019 The MLIR Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================
//
// These classes provide the ability to relate MLIR objects back to source
// location position information.
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_IR_LOCATION_H
#define MLIR_IR_LOCATION_H

#include "mlir/IR/Attributes.h"

namespace mlir {

class Attribute;
class MLIRContext;
class Identifier;

namespace detail {

struct LocationStorage;
struct UnknownLocationStorage;
struct FileLineColLocationStorage;
struct NameLocationStorage;
struct CallSiteLocationStorage;
struct FusedLocationStorage;

} // namespace detail

/// Location objects represent source locations information in MLIR.
/// LocationAttr acts as the anchor for all Location based attributes.
class LocationAttr : public Attribute {
public:
  using Attribute::Attribute;

  /// Methods for support type inquiry through isa, cast, and dyn_cast.
  static bool classof(Attribute attr) {
    return attr.getKind() >= StandardAttributes::FIRST_LOCATION_ATTR &&
           attr.getKind() <= StandardAttributes::LAST_LOCATION_ATTR;
  }
};

/// This class defines the main interface for locations in MLIR and acts as a
/// non-nullable wrapper around a LocationAttr.
class Location {
public:
  Location(LocationAttr loc) : impl(loc) {
    assert(loc && "location should never be null.");
  }

  /// Access the impl location attribute.
  operator LocationAttr() const { return impl; }
  LocationAttr *operator->() const { return const_cast<LocationAttr *>(&impl); }

  /// Type casting utilities on the underlying location.
  template <typename U> bool isa() const { return impl.isa<U>(); }
  template <typename U> U dyn_cast() const { return impl.dyn_cast<U>(); }
  template <typename U> U cast() const { return impl.cast<U>(); }

  /// Comparison operators.
  bool operator==(Location rhs) const { return impl == rhs.impl; }
  bool operator!=(Location rhs) const { return !(*this == rhs); }

  /// Print the location.
  void print(raw_ostream &os) const { impl.print(os); }
  void dump() const { impl.dump(); }

  friend ::llvm::hash_code hash_value(Location arg);

  /// Methods for supporting PointerLikeTypeTraits.
  const void *getAsOpaquePointer() const { return impl.getAsOpaquePointer(); }
  static Location getFromOpaquePointer(const void *pointer) {
    return LocationAttr(reinterpret_cast<const AttributeStorage *>(pointer));
  }

protected:
  /// The internal backing location attribute.
  LocationAttr impl;
};

inline raw_ostream &operator<<(raw_ostream &os, const Location &loc) {
  loc.print(os);
  return os;
}

/// Represents a location as call site. "callee" is the concrete location
/// (Unknown/NameLocation/FileLineColLoc) and "caller" points to the caller's
/// location (another CallLocation or a concrete location). Multiple
/// CallSiteLocs can be chained to form a call stack.
class CallSiteLoc
    : public Attribute::AttrBase<CallSiteLoc, LocationAttr,
                                 detail::CallSiteLocationStorage> {
public:
  using Base::Base;

  /// Return a uniqued call location object.
  static CallSiteLoc get(Location callee, Location caller,
                         MLIRContext *context);

  /// Return a call site location which represents a name reference in one line
  /// or a stack of frames. The input frames are ordered from innermost to
  /// outermost.
  static CallSiteLoc get(Location name, ArrayRef<Location> frames,
                         MLIRContext *context);

  /// The concrete location information this object presents.
  Location getCallee() const;

  /// The caller's location.
  Location getCaller() const;

  /// Methods for support type inquiry through isa, cast, and dyn_cast.
  static bool kindof(unsigned kind) {
    return kind == StandardAttributes::CallSiteLocation;
  }
};

/// Represents a location derived from a file/line/column location.  The column
/// and line may be zero to represent unknown column and/or unknown line/column
/// information.
class FileLineColLoc
    : public Attribute::AttrBase<FileLineColLoc, LocationAttr,
                                 detail::FileLineColLocationStorage> {
public:
  using Base::Base;

  /// Return a uniqued FileLineCol location object.
  static FileLineColLoc get(Identifier filename, unsigned line, unsigned column,
                            MLIRContext *context);
  static FileLineColLoc get(StringRef filename, unsigned line, unsigned column,
                            MLIRContext *context);

  StringRef getFilename() const;

  unsigned getLine() const;
  unsigned getColumn() const;

  /// Methods for support type inquiry through isa, cast, and dyn_cast.
  static bool kindof(unsigned kind) {
    return kind == StandardAttributes::FileLineColLocation;
  }
};

/// Represents a value composed of multiple source constructs, with an optional
/// metadata attribute.
class FusedLoc : public Attribute::AttrBase<FusedLoc, LocationAttr,
                                            detail::FusedLocationStorage> {
public:
  using Base::Base;

  /// Return a uniqued Fused Location object. The first location in the list
  /// will get precedence during diagnostic emission, with the rest being
  /// displayed as supplementary "fused from here" style notes.
  static LocationAttr get(ArrayRef<Location> locs, Attribute metadata,
                          MLIRContext *context);
  static LocationAttr get(ArrayRef<Location> locs, MLIRContext *context) {
    return get(locs, Attribute(), context);
  }

  ArrayRef<Location> getLocations() const;

  /// Returns the optional metadata attached to this fused location. Given that
  /// it is optional, the return value may be a null node.
  Attribute getMetadata() const;

  /// Methods for support type inquiry through isa, cast, and dyn_cast.
  static bool kindof(unsigned kind) {
    return kind == StandardAttributes::FusedLocation;
  }
};

/// Represents an identity name attached to a child location.
class NameLoc : public Attribute::AttrBase<NameLoc, LocationAttr,
                                           detail::NameLocationStorage> {
public:
  using Base::Base;

  /// Return a uniqued name location object. The child location must not be
  /// another NameLoc.
  static NameLoc get(Identifier name, Location child, MLIRContext *context);

  /// Return a uniqued name location object with an unknown child.
  static NameLoc get(Identifier name, MLIRContext *context);

  /// Return the name identifier.
  Identifier getName() const;

  /// Return the child location.
  Location getChildLoc() const;

  /// Methods for support type inquiry through isa, cast, and dyn_cast.
  static bool kindof(unsigned kind) {
    return kind == StandardAttributes::NameLocation;
  }
};

/// Represents an unknown location.  This is always a singleton for a given
/// MLIRContext.
class UnknownLoc : public Attribute::AttrBase<UnknownLoc, LocationAttr> {
public:
  using Base::Base;

  /// Get an instance of the UnknownLoc.
  static UnknownLoc get(MLIRContext *context);

  /// Methods for support type inquiry through isa, cast, and dyn_cast.
  static bool kindof(unsigned kind) {
    return kind == StandardAttributes::UnknownLocation;
  }
};

// Make Location hashable.
inline ::llvm::hash_code hash_value(Location arg) {
  return hash_value(arg.impl);
}

} // end namespace mlir

namespace llvm {

// Type hash just like pointers.
template <> struct DenseMapInfo<mlir::Location> {
  static mlir::Location getEmptyKey() {
    auto pointer = llvm::DenseMapInfo<void *>::getEmptyKey();
    return mlir::Location::getFromOpaquePointer(pointer);
  }
  static mlir::Location getTombstoneKey() {
    auto pointer = llvm::DenseMapInfo<void *>::getTombstoneKey();
    return mlir::Location::getFromOpaquePointer(pointer);
  }
  static unsigned getHashValue(mlir::Location val) {
    return mlir::hash_value(val);
  }
  static bool isEqual(mlir::Location LHS, mlir::Location RHS) {
    return LHS == RHS;
  }
};

/// We align LocationStorage by 8, so allow LLVM to steal the low bits.
template <> struct PointerLikeTypeTraits<mlir::Location> {
public:
  static inline void *getAsVoidPointer(mlir::Location I) {
    return const_cast<void *>(I.getAsOpaquePointer());
  }
  static inline mlir::Location getFromVoidPointer(void *P) {
    return mlir::Location::getFromOpaquePointer(P);
  }
  enum {
    NumLowBitsAvailable =
        PointerLikeTypeTraits<mlir::Attribute>::NumLowBitsAvailable
  };
};

} // namespace llvm

#endif
