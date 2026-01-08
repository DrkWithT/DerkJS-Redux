/*
 Test basic object operations:
 Get / Set properties & display objects.
 */

var bob = {
    id: 12340,
    income_k: 45.75,
    has_job: true
};

bob.income_k = bob.income_k + 1.25;

return bob.income_k;
