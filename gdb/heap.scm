(define-module (heap)
  #:use-module (gdb)
  #:use-module (util)
  #:use-module ((linked-list) #:prefix ll/))

(define-public (listify-tags)
  (ll/listify-nodes
    (let ((tag-ptr (symbol-value (car (lookup-symbol "gp_heap_start")))))
      (if (equal? tag-ptr null-ptr)
        #f
        (value-dereference tag-ptr)))))

(define-public (print-tag tag)
  (let* ((addr (if (value-is-pointer? tag)
                 (value->integer tag)
                 (value->integer (value-address tag))))
         (used? (value->bool (value-field tag "b_used")))
         (size (value->integer (value-field tag "size"))))
    (format #t "at 0x~8,'0x, ~a, ~d bytes\n"
            addr (if used? "used" "free") size)))
