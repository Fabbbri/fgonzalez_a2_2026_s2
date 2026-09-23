# Deep Learning + ThreadLevel Parallelism

## Instalación

```bash
clang++ --version

brew install eigen
brew info eigen
```

## ¿Qué busca Deep Learning?

El objetivo: tomar un batch de entradas $x$, pasarlas por una serie de capas ocultas y obtener una salida $\hat{y}$ lo más cercana posible a una predicción $y$

Podemos ver un ejemplo para un vector $x$ de 2 elementos, en este caso, el proceso de una etapa de entrenamiento consiste en:

```
x -> forward pass 1 -> capa(s) oculta(s) -> forward pass 2 -> salida
```

Se utilizarán matrices de pesos y vectores de sesgo. Al principio se definen como valores aleatorios cualquiera.

## Forward pass - p1

Forward pass es el proceso de transformar la entrada y aplicarle una función de activación.

Llamemos al vector de entrada transformado (pero sin aplicarle la función de activación) $z_1$, y se le llama $a_1$ (en la literatura se le llama también $\phi$) cuando se le aplica la función de activación.

```math
x \rightarrow z_1 = xW_1 + b_1
```

Note que la operación hecha es producto punto, la cuál es la multiplicación de `Eigen`.

```math
z_1 \rightarrow a_1 = \sigma(z_1)
```

Con $\sigma$ la función `sigmoid` o `RELU`. Para `sigmoid` se tiene que es:

```math
\sigma(z)=\frac{1}{1+e^{−z}}
```

Así, $a_1$ es el valor del `hidden layer` que se le pasará a la salida.

## Forward pass - p2

Ahora debe aplicarse el mismo proceso para pasar el valor de hidden layer como entrada al nodo de salida. 

Se hace lo mismo: transformar la entrada y aplicarle una función de activación.

```math
a_1 \rightarrow z_2 = a_1W_2 + b_2
```

```math
z_2 \rightarrow a_2 = \sigma(z_2)
```

Así, se tiene listo el nodo de salida:

```math
a_2 = \hat{y}
```

## Backpropagation

Teniendo la salida del modelo, hay que verificar que tan alejado es de la predicción establecida. Se puede utilizar el `Mean Squared Error (MSE)`, que simplemente calcula el cuadrado de la diferencia.

```math
MSE = \frac{1}{N} \sum_{​i=1}^N​(y_i​ − \hat{y_i}​)^2
```

Para una sola muestra, $N=1$

Teniendo el error, es posible trabajar desde la salida hacia atrás para averiguar cuanto cambia cada peso y sesgo el `MSE`. Esto es lo que significa la función $L$, la `Loss Function` (función de cambio).

Computacionalmente hablando, podemos usar los resultados conocidos de las derivadas para aproximar la `Loss Function`. Recuerde que en cálculo, la derivada representa el cambio ante un cambio muy pequeño. 

Así, podemos aplicar derivadas para descubrir que tanto cambia el error si se varían los pesos ($W_1$, $W_2$) y sesgos ($b_1$, $b_2$) que se definen al principio. De esta forma podemos saber que tan significativo es un parámetro en cuánto a la influencia sobre el cambio se refiere.


### Variación del error MSE respecto a $W_2$

Observemos cuanto cambia la pérdida $L$ (dada por el MSE)

```math
\frac{\partial L }{\partial W_2}
```

Note que es una derivada parcial que se puede representar (usando regla de la cadena) como:

```math
\frac{\partial L}{\partial W_2} = \frac{\partial L}{\partial z_2} a_1^T
```

A su vez, la derivada parcial de la derecha se puede representar como: 

```math
\frac{\partial L}{\partial z_2} = \frac{\partial L}{\partial \hat{y}} \frac{\partial \hat{y}}{\partial z_2}
```

Ahora bien, estas derivadas se pueden calcular por fuera. Se tiene que:

1. Error respecto a la salida

```math
L=(y−\hat{y})^2
```

La derivada es sencilla:

```math
\frac{\partial L}{\partial \hat{y}} = 2(y−\hat{y})
```


2. Salida respecto a $z_2$

```math
\hat{y} = a_2 = \sigma(z_2)
```

Se conoce que la derivada de sigmoid es:

```math
\sigma'(z)= \sigma(z)(1−\sigma(z))
````

Por lo tanto:

```math
\frac{\partial \hat{y}}{\partial z_2} = \hat{y}(1-\hat{y})
```

3. Juntar resultados

Entonces, llegamos a una expresión simplificada:

```math
\frac{\partial L}{\partial W_2} = 2(y−\hat{y}) (\hat{y}(1-\hat{y})) (a_1^T)
```

### Variación del error MSE respecto a $b_2$

Se tiene la ecuación:

```math
z_2 = a_1W_2 + b_2
```

Note que, si se deriva respecto a $b_2$, $a_1W_2$ queda como constante. Por lo tanto:

```math
\frac{\partial L}{\partial b_2} = \frac{\partial L}{\partial z_2} = \hat{y}(1-\hat{y})
```

### Variación del error MSE respecto a $W_1$

```math
\frac{\partial L }{\partial W_1}
```

Note que es una derivada parcial que se puede representar (usando regla de la cadena) como:

```math
\frac{\partial L}{\partial W_1} = \frac{\partial L}{\partial z_1} x^T
```

Es posible expandir la derivada parcial del lado derecho ($z_1$) con regla de la cadena:

```math
\frac{\partial L}{\partial z_1} = \frac{\partial L}{\partial a_1} \odot \frac{\partial a_1}{\partial z_1}
```

Con $\odot$ la multiplicación `element wise`.

Es posible definir las nuevas derivadas parciales del lado derecho:

1. Derivada de $a_1$ respecto a $z_1$

Se sabe que:

```math
a_1 = \sigma(z_1)
```

Por lo que la derivada es

```math
\frac{\partial a_1}{\partial z_1} = a_1(1-a_1)
```

2. Derivada de $L$ respecto a $a_1$

Se sabe que: 

```math
z_2 = a_1W_2 + b_2
```

Entonces, para saber cuanto afecta $a_1$ al error, reacomodando la ecuación:

```math
\frac{\partial L}{\partial a_1} = W_2^T\frac{\partial L}{\partial z_2}
```

(b es constante entonces desaparece)

3. Así, puede reescribirse la ecuación como:

```math
\frac{\partial L}{\partial W_1} = (W_2^T\frac{\partial L}{\partial z_2}) \odot (a_1(1-a_1))x^T 
```

Y a su vez:

```math
\frac{\partial L}{\partial W_1} = (W_2^T(\hat{y}(1-\hat{y}))) \odot (a_1(1-a_1))x^T
```

### Variación del error MSE respecto a $b_1$

Se tiene la ecuación:

```math
z_1 = W_1x + b_1
```

Note que, si se deriva respecto a $b_1$, $W_1x$ queda como constante. Por lo tanto:

```math
\frac{\partial L}{\partial b_1} = \frac{\partial L}{\partial z_1} = (W_2^T(\hat{y}(1-\hat{y}))) \odot (a_1(1-a_1))
```

## Descenso de Gradiente

Ahora que se tienen las derivadas se conoce computacionalmente cuánto ha variado la salida obtenida del resultado deseado y cuánto ha influenciado cada parámetro en esa diferencia.

Estas derivadas de la `Loss` function respecto a:

`dW1
db1
dW2
db2`

Se les llaman `gradientes` e indican en qué dirección cambia el error si modificamos cada parámetro.

Así, es posible utilizar la función llamada: `descenso de gradiente`.

Esta función, de forma simple: indica en que dirección debe moverse la función para llegar a un mínimo local (o sea, reducir la función, en este caso `Loss`, lo más posible).

De esa forma, lo que se obtiene es reducir el error respecto a la predicción deseada actualizando los parámetros mediante el descenso de gradiente. Para los parámetros `\theta` (peso o bias), el descenso de gradiente consiste en:

```math
\theta_{nuevo} = \theta_{actual} - \eta \frac{\partial L}{\partial \theta}
```

donde $\eta$ es llamado el Learning Rate, y es (por ilustrarlo meramente) que tanto o que tan rápido nos movemos (descendemos) en la dirección (dada por el gradiente $\frac{\partial L}{\partial \theta}$)

Note que es llamado descenso porque hay un $-$ (o sea, nos estamos moviendo negativamente en la dirección dada por el gradiente). Normalmente, $\eta$ no debe ser muy grande para que al acercarse al mínimo local no se sobre pase por accidente (irse acercando poco a poco con precisión).

Un $\eta = 0.1$ es aceptable

## Repetir entrenamiento

Todo este proceso previo consiste en 1 sola etapa de entrenamiento de un modleo. Los modelos profesionales/modernos de `Deep Learning` generan miles de iteraciones de entrenamiento para que el propio modelo `aprenda` a optimizar sus pesos y sesgos.

Computacionalmente hablando, esto puede hacerse con un bucle for sencillo:

```C
for (int epoch = 0; epoch < 1000; epoch++) {
    // 1. Forward-pass
    // 2. MSE
    // 3. Backpropagation
    // 4. Actualizar pesos
}
```
