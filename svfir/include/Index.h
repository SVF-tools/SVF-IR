+#ifndef INDEX_H
+#define INDEX_H
+
+namespace SVFIR {
+
+/// Index into various vectors, like the vector of functions in a program.
+/// This index is often used as a key, like a pointer often is (with the
+/// added benefit of being able to use vectors instead of maps).
+using Index = size_t;
+
+}  // namespace SVFIR
+
+#endif  // INDEX_H
