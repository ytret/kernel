(define-module (linked-list)
   #:use-module (gdb)
   #:use-module (util))

(define-public (node->struct node struct-type field-name)
  (value-cast
    (make-value (- (value->integer (value-address node))
                   (get-field-offset struct-type field-name)))
    (type-pointer struct-type)))

(define-public (get-next-node node)
  (let ((next-ptr (value-field node "p_next")))
    (if (equal? next-ptr null-ptr) #f (value-dereference next-ptr))))

(define-public (listify-nodes node)
  (if node
      (let ((next-node (get-next-node node)))
        (append (list node)
                (if next-node (listify-nodes next-node) '())))
      '()))

(define-public (length list-val)
  (let ((first-node-ptr (value-field list-val "p_first_node")))
    (if (equal? first-node-ptr null-ptr)
      0
      (length (listify-nodes (value-dereference first-node-ptr))))))
