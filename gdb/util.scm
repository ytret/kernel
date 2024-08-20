(define-module (util)
  #:use-module (gdb))

(define-public type/task (lookup-type "task_t"))
(define-public type/list (lookup-type "list_t"))
(define-public type/list-node (lookup-type "list_node_t"))
(define-public null-ptr (make-value 0))

(define-public (get-field-offset struct-type field-name)
  (let* ((zero-ptr (value-cast (make-value 0) (type-pointer struct-type)))
         (field (value-field (value-dereference zero-ptr) field-name)))
    (value->integer (value-address field))))

(define-public (value-is-pointer? value)
  (eq? TYPE_CODE_PTR (type-code (value-type value))))
